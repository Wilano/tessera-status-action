#include "status_action.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace esphome::status_action {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

static std::string lower(const std::string &in) {
  std::string out = in;
  for (char &c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

// A state falls in one of three groups, so a person can colour or icon the states together: the resting ones, the
// engaged ones, and the alert ones (the alarm fired or is on its way to armed).
enum class Bucket { CALM = 0, ACTIVE = 1, ALERT = 2 };

// A chosen icon arrives as its five-hex-digit Material Design Icons codepoint ("F033E"); anything else is 0, and the
// tile keeps its dot. The firmware's icon fonts carry only Tessera's set (plugin.yaml lists the names), so a codepoint
// outside the private-use area, or one not in that set, draws nothing.
static uint32_t icon_cp(const std::string &hex) {
  if (hex.size() != 5) return 0;
  uint32_t v = 0;
  for (char c : hex) {
    const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10
                  : c >= 'a' && c <= 'f'                                 ? c - 'a' + 10
                                                                         : -1;
    if (d < 0) return 0;
    v = v << 4 | static_cast<uint32_t>(d);
  }
  return v >= 0xF0000 && v <= 0xFFFFD ? v : 0;
}

// The option's word for a colour, to a tone; an unknown word (including "default") leaves the state's built-in tone.
static bool color_tone(const std::string &word, Tone &out) {
  if (word == "grey") out = Tone::NORMAL;
  else if (word == "green") out = Tone::GOOD;
  else if (word == "blue") out = Tone::ACCENT;
  else if (word == "amber") out = Tone::BUSY;
  else if (word == "red") out = Tone::ALERT;
  else return false;
  return true;
}

// The tile: an entity's state up top, and a tap that acts on a second entity (a toggle, a script, a scene, a button).
// create() reads the options and makes the parts; on_state keeps the chosen entities' states; on_tick paints them.
class ControlTile : public tessera::Tile {
 public:
  explicit ControlTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    tile_name_ = c.name;
    // The target's entity id and the service the tap sends travel in the action itself, so they are read once here.
    target_id_ = c.options["target"] | "";
    const std::string verb = c.options["action"] | "toggle";
    if (verb == "turn_on") service_ = "homeassistant.turn_on";
    else if (verb == "turn_off") service_ = "homeassistant.turn_off";
    else if (verb == "press") service_ = "button.press";
    else service_ = "homeassistant.toggle";
    confirm_opt_ = c.options["confirm"] | false;
    confirm_de_ = std::string(c.options["confirm_lang"] | "en") == "de";
    big_ = std::string(c.options["text_size"] | "large") != "small";
    show_foot_ = c.options["show_action"] | true;
    // The per-group colour and icon overrides, read once. A group whose colour stays "default" keeps the state's own
    // built-in tone; the alert group has no colour override, so an alarm always keeps its red or amber.
    color_set_[int(Bucket::CALM)] = color_tone(std::string(c.options["color_calm"] | "default"), color_ov_[int(Bucket::CALM)]);
    color_set_[int(Bucket::ACTIVE)] = color_tone(std::string(c.options["color_active"] | "default"), color_ov_[int(Bucket::ACTIVE)]);
    color_set_[int(Bucket::ALERT)] = false;
    icon_cp_[int(Bucket::CALM)] = icon_cp(std::string(c.options["icon_calm"] | "none"));
    icon_cp_[int(Bucket::ACTIVE)] = icon_cp(std::string(c.options["icon_active"] | "none"));
    icon_cp_[int(Bucket::ALERT)] = icon_cp(std::string(c.options["icon_alert"] | "none"));

    dot_ = ui::block(c.parent, theme::MUTED);
    icon_ = ui::label(c.parent, Font::ICON_SMALL, theme::MUTED);
    name_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    state_ = ui::label(c.parent, Font::HEADLINE, theme::INK);
    foot_ = ui::label(c.parent, Font::BODY, theme::MUTED);
  }

  void on_state(JsonObjectConst data) override {
    JsonObjectConst ents = data["entities"];
    JsonObjectConst s = ents["status"];
    have_status_ = !s.isNull();
    status_state_ = have_status_ ? (s["state"] | "") : "";
    status_name_ = have_status_ ? (s["name"] | "") : "";
    status_unit_ = have_status_ ? (s["attributes"]["unit_of_measurement"] | "") : "";
    JsonObjectConst t = ents["target"];
    target_name_ = t.isNull() ? "" : (t["name"] | "");
  }

  void on_tick(uint32_t epoch) override {
    epoch_ = epoch;
    if (confirming_ && epoch && epoch - confirm_since_ >= kConfirmWindow) confirming_ = false;
    if (acting_ && epoch && epoch - act_since_ >= kActFlash) acting_ = false;
    paint_();
  }

  void on_theme() override { paint_(); }

  void on_tap() override {
    if (target_id_.empty()) return;  // nothing to act on yet
    if (confirm_opt_ && !confirming_) {
      confirming_ = true;
      confirm_since_ = epoch_;
      paint_();
      return;
    }
    confirming_ = false;
    fire_();
    acting_ = true;
    act_since_ = epoch_;
    paint_();
  }

 private:
  static constexpr uint32_t kConfirmWindow = 4;  // seconds the second tap has
  static constexpr uint32_t kActFlash = 2;       // seconds the dot stays busy after a tap

  void fire_() {
    JsonDocument d;
    d["entity_id"] = target_id_;
    // The entity id is in the fields, so the action's own entity stays empty: this reaches a script, scene or button too.
    tessera::action(service_.c_str(), std::string(), d.as<JsonVariantConst>());
  }

  // The word and the colour a state shows as: the alarm's own states, the common on/off, open/closed, locked words,
  // and any other state as it is (a number keeps its unit).
  std::pair<std::string, Tone> state_word_() const {
    const std::string k = lower(status_state_);
    struct M { const char *s; const char *key; Tone t; };
    static const M map[] = {
        {"disarmed", "s_disarmed", Tone::GOOD},
        {"armed_home", "s_armed_home", Tone::ACCENT},
        {"armed_away", "s_armed_away", Tone::ACCENT},
        {"armed_night", "s_armed_night", Tone::ACCENT},
        {"armed_vacation", "s_armed", Tone::ACCENT},
        {"armed_custom_bypass", "s_armed", Tone::ACCENT},
        {"arming", "s_arming", Tone::BUSY},
        {"pending", "s_pending", Tone::BUSY},
        {"triggered", "s_triggered", Tone::ALERT},
        {"on", "s_on", Tone::ACCENT},
        {"off", "s_off", Tone::NORMAL},
        {"open", "s_open", Tone::BUSY},
        {"opening", "s_open", Tone::BUSY},
        {"closed", "s_closed", Tone::GOOD},
        {"closing", "s_closed", Tone::GOOD},
        {"locked", "s_locked", Tone::GOOD},
        {"unlocked", "s_unlocked", Tone::ACCENT},
        {"locking", "s_locked", Tone::BUSY},
        {"unlocking", "s_unlocked", Tone::BUSY},
        {"home", "s_home", Tone::GOOD},
        {"not_home", "s_away", Tone::NORMAL},
        {"idle", "s_idle", Tone::NORMAL},
        {"unavailable", "s_unavailable", Tone::NORMAL},
        {"unknown", "s_unknown", Tone::NORMAL},
    };
    for (const M &m : map)
      if (k == m.s) return {std::string(plugin_->text(m.key)), m.t};
    std::string out = status_state_;
    if (!out.empty()) out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    if (!status_unit_.empty()) out += " " + status_unit_;
    return {out, Tone::NORMAL};
  }

  // Which group the state is in, for the colour and icon overrides.
  Bucket bucket_() const {
    const std::string k = lower(status_state_);
    if (k == "triggered" || k == "arming" || k == "pending") return Bucket::ALERT;
    if (k == "on" || k.rfind("armed", 0) == 0 || k == "open" || k == "opening" || k == "unlocked" || k == "unlocking")
      return Bucket::ACTIVE;
    return Bucket::CALM;
  }

  void paint_() {
    std::string top = !tile_name_.empty() ? tile_name_ : status_name_;
    std::string center;
    Tone tone = Tone::NORMAL;
    uint32_t cp = 0;  // the icon for this state, 0 for the plain dot
    if (confirming_) {
      center = plugin_->text(confirm_de_ ? "confirm_tap_de" : "confirm_tap");
      tone = Tone::BUSY;
    } else if (!have_status_) {
      center = plugin_->text(target_id_.empty() ? "pick_status" : "no_status");
      top.clear();
    } else {
      auto r = state_word_();
      center = r.first;
      tone = r.second;
      const int bi = int(bucket_());
      if (color_set_[bi]) tone = color_ov_[bi];  // the chosen colour, or the state's own
      cp = icon_cp_[bi];                          // the chosen icon, or 0 for the dot
    }
    const std::string foot = (show_foot_ && !target_name_.empty() && !confirming_) ? target_name_ : std::string();

    // Top row: a dot in the state's colour, or a per-state icon in its place, the name beside it.
    const int head = top.empty() ? 0 : ui::line_height(Font::BODY);
    const int dot = ui::px(ui::large() ? 12 : 8);
    const std::string glyph = cp ? ui::icon(cp) : std::string();
    const bool use_icon = head && !glyph.empty();
    const int icon_h = ui::line_height(Font::ICON_SMALL);
    const int lead = use_icon ? ui::text_width(glyph, Font::ICON_SMALL) : dot;
    const Tone mark_tone = acting_ ? Tone::BUSY : tone;
    // the dot, shown unless an icon takes its place
    ui::set_fill(dot_, mark_tone, theme::MUTED);
    lv_obj_set_size(dot_, dot, dot);
    lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_opa(dot_, (head && !use_icon) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_pos(dot_, 0, (head - dot) / 2);
    // the icon, shown in the dot's place when the state's group has one
    ui::set_tone(icon_, mark_tone, theme::MUTED);
    ui::set_text(icon_, use_icon ? glyph : std::string());
    lv_obj_set_size(icon_, use_icon ? std::max(1, lead) : 1, use_icon ? icon_h : 1);
    lv_obj_set_pos(icon_, 0, use_icon ? (head - icon_h) / 2 : 0);
    ui::set_color(name_, theme::MUTED);
    ui::set_text(name_, top);
    lv_obj_set_pos(name_, head ? lead + ui::px(6) : 0, 0);
    lv_obj_set_size(name_, head ? std::max(1, width_ - lead - ui::px(6)) : width_, head ? head : 1);

    const int foot_h = foot.empty() ? 0 : ui::line_height(Font::BODY);
    const int band = std::max(1, height_ - head - foot_h);

    // The biggest of the screen's fonts the state fits in, both across and down. "Small" starts one step down, so a
    // short state sits calmer with more room around it.
    static const Font big_fonts[] = {Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE};
    static const Font small_fonts[] = {Font::TITLE, Font::BODY_LARGE, Font::BODY};
    Font face = big_ ? Font::BODY_LARGE : Font::BODY;
    const Font *fonts = big_ ? big_fonts : small_fonts;
    const int n = big_ ? 4 : 3;
    for (int i = 0; i < n; ++i) {
      const Font f = fonts[i];
      if (ui::text_width(center, f) <= width_ && ui::line_height(f) <= band) { face = f; break; }
    }
    ui::set_font(state_, face);
    ui::set_tone(state_, tone, theme::INK);
    ui::set_text(state_, center);
    lv_obj_set_width(state_, width_);
    lv_obj_set_style_text_align(state_, LV_TEXT_ALIGN_CENTER, 0);
    const int sh = ui::line_height(face);
    lv_obj_set_height(state_, sh);
    lv_obj_set_pos(state_, 0, head + std::max(0, (band - sh) / 2));

    ui::set_color(foot_, theme::MUTED);
    ui::set_text(foot_, foot);
    lv_obj_set_width(foot_, width_);
    lv_obj_set_style_text_align(foot_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_height(foot_, foot_h ? foot_h : 1);
    lv_obj_set_pos(foot_, 0, height_ - (foot_h ? foot_h : 1));
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0;
  std::string tile_name_, target_id_, service_;
  bool confirm_opt_ = false, confirm_de_ = false, big_ = true, show_foot_ = true;
  bool color_set_[3] = {false, false, false};
  Tone color_ov_[3] = {Tone::NORMAL, Tone::NORMAL, Tone::NORMAL};
  uint32_t icon_cp_[3] = {0, 0, 0};
  bool have_status_ = false;
  std::string status_state_, status_name_, status_unit_, target_name_;
  bool confirming_ = false, acting_ = false;
  uint32_t epoch_ = 0, confirm_since_ = 0, act_since_ = 0;
  lv_obj_t *dot_{}, *icon_{}, *name_{}, *state_{}, *foot_{};
};

void StatusAction::setup() {
  add_tile("control", [this]() { return new ControlTile(this); });
}

}  // namespace esphome::status_action
