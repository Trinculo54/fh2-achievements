// The in-game Achievements page (an FH2ML page of the mod's own design): what the pause
// menu's ACHIEVEMENTS tile opens.
//
//   ACHIEVEMENTS                                   [trophy 21 / 88   330 / 2000G]
//   +---------------------------+  +------------------------------------------------+
//   | RECENTLY UNLOCKED   3     |  | FESTIVAL & ROAD TRIPS           7 / 9   175G   |
//   | FESTIVAL & ...    7 / 9   |  |------------------------------------------------|
//   | WRISTBANDS ...    2 / 8   |  | [trophy] Welcome to Sisteron               25G  |
//   | ...                       |  |          Arrive at Sisteron for the first time. |
//   +---------------------------+  |          UNLOCKED 2026-10-19                    |
//                                  | [lock]   Horizon Leader                    20G  |
//                                  |          ...  [=======-----] 12 / 50 roads     |
//                                  +------------------------------------------------+
//
// Left, the sections: the page's slot 0 is the list's SelectedIndex, which follows the focus
// (the game's list items are selected only while focused, so it's -1 while the focus is on the
// cards). Slot 2 is the section shown, set from slot 0 when it's a section (achievements.cpp), and
// only that section's cards show on the right (FH2ML_Slot_2_Is_<k>). A on a section moves the
// focus into its cards, B comes back; slot 1 says which side has the focus, for the help bar. Everything is the game's own controls, styles and textures (UI.zip,
// UI\textures\Anthem.zip), written out as static XAML each time the rows change.
#include "ach.hpp"
#include "FH2ML_v4.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ach {

namespace {

// ---- text -------------------------------------------------------------------------------------

std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default: o += c;
        }
    return o;
}

std::string fmt(const char* f, double v) {
    char b[48];
    std::snprintf(b, sizeof b, f, v);
    return b;
}

// 1234567 -> "1,234,567"; from 100,000 up, shorter: "812k", "1.41M"
std::string number(double v) {
    if (v >= 1e6) {
        std::string s = fmt("%.2f", v / 1e6);
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
        return s + "M";
    }
    if (v >= 1e5) return fmt("%.0fk", std::floor(v / 1e3));
    const std::string s = fmt("%.0f", std::floor(v));
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i && (s.size() - i) % 3 == 0) o += ',';
        o += s[i];
    }
    return o;
}

std::string gs(int g) { return std::to_string(g) + "G"; }

// ---- the sections ---------------------------------------------------------------------------

struct SectionDef {
    const char* title;
    std::vector<std::string> ids;   // empty: by group (Storm Island, Porsche), or the recent ones
    int group = -1;
};

const std::vector<SectionDef>& section_defs() {
    static const std::vector<SectionDef> s = {
        {"RECENTLY UNLOCKED", {}},
        {"FESTIVAL & ROAD TRIPS",
         {"welcome_festival", "welcome_sisteron", "welcome_nice", "welcome_saintmartin", "welcome_castelletto", "welcome_montellino",
          "welcome_sangiovanni", "horizon_leader", "wanderlust"}},
        {"WRISTBANDS & SKILLS",
         {"wristband_yellow", "wristband_green", "wristband_pink", "wristband_purple", "wristband_gold", "license_to_skill",
          "stone_cold_skiller", "fast_and_clean"}},
        {"RACING",
         {"first_of_many", "number_one", "horizon_fan", "horizon_enthusiast", "all_your_race", "horizon_champion", "train_in_vain",
          "multi_disciplined", "rain_master", "wild_drivatar", "we_meet_again"}},
        {"CARS & GARAGE", {"new_car_smell", "car_lover", "worthy_sultan", "storage_hunter", "photographer"}},
        {"EXPLORATION & BUCKET LIST", {"summer_sale", "smash_happy", "human_gps", "one_down", "kick_the_bucket", "rest_in_peace"}},
        {"ONLINE",
         {"friends_with_benefits", "super_meet_boy", "bragging_rights", "social_caterpillar", "social_butterfly", "team_player", "loyal",
          "on_the_road_again", "well_travelled", "yes_we_can", "you_are_pg"}},
        {"STORM ISLAND", {}, static_cast<int>(Group::StormIsland)},
        {"PORSCHE EXPANSION", {}, static_cast<int>(Group::Porsche)},
    };
    return s;
}

constexpr size_t kRecent = 8;   // RECENTLY UNLOCKED shows this many

enum Order { kUnlocked, kLocked, kOnline, kPending };

struct Card {
    const Def* d;
    Order order;
    Progress p;
    bool measured;
};

Card card_for(const Def& d, const Facts* f, const std::map<std::string, std::string>& unlocked) {
    Card c{&d, kLocked, {}, false};
    if (d.eval && f) {
        c.p        = d.eval(*f);
        c.measured = d.unit != nullptr;
    }
    if (unlocked.count(d.id)) c.order = kUnlocked;
    else if (d.track == Track::Online) c.order = kOnline;
    else if (d.track == Track::Pending) c.order = kPending;
    return c;
}

const Def* find_def(const std::string& id) {
    for (const Def& d : defs())
        if (id == d.id) return &d;
    return nullptr;
}

// ---- XAML ---------------------------------------------------------------------------------------

// Text styles: the game's, in the list's dark grey (or a lighter one), turning the selected
// row's white when it has the focus, as the options lists' text does.
// `shrink`: one line, made smaller when it doesn't fit (as the options lists' labels are).
std::string text_style(const char* key, const char* based_on, const char* color, bool shrink = false) {
    return std::string("            <Style x:Key=\"") + key + "\" TargetType=\"local:FZTextBlock\" BasedOn=\"{StaticResource " + based_on +
           "}\">\r\n"
           "                <Setter Property=\"Color\" Value=\"" + color + "\" />\r\n" +
           (shrink ? "                <Setter Property=\"Stretch\" Value=\"Uniform\" />\r\n"
                     "                <Setter Property=\"StretchDirection\" Value=\"DownOnly\" />\r\n"
                   : "") +
           "                <Style.Triggers>\r\n"
           "                    <DataTrigger Binding=\"{Binding RelativeSource={RelativeSource Mode=FindAncestor, AncestorType=ListBoxItem}, "
           "Path=IsKeyboardFocusWithin}\" Value=\"True\">\r\n"
           "                        <Setter Property=\"Color\" Value=\"{StaticResource A_Color_TileSelected_Text}\" />\r\n"
           "                    </DataTrigger>\r\n"
           "                </Style.Triggers>\r\n"
           "            </Style>\r\n";
}

// A material (a bar, an icon) in one colour, and another while its row has the focus.
std::string material_style(const char* key, const char* color, const char* focused, const char* extra = "") {
    return std::string("            <Style x:Key=\"") + key + "\" TargetType=\"local:FZMaterial\">\r\n"
           "                <Setter Property=\"Color\" Value=\"" + color + "\" />\r\n" + extra +
           "                <Style.Triggers>\r\n"
           "                    <DataTrigger Binding=\"{Binding RelativeSource={RelativeSource Mode=FindAncestor, AncestorType=ListBoxItem}, "
           "Path=IsKeyboardFocusWithin}\" Value=\"True\">\r\n"
           "                        <Setter Property=\"Color\" Value=\"" + focused + "\" />\r\n"
           "                    </DataTrigger>\r\n"
           "                </Style.Triggers>\r\n"
           "            </Style>\r\n";
}

std::string resources() {
    const char* white  = "{StaticResource A_Color_TileSelected_Text}";
    const char* orange = "{StaticResource A_Color_TileSelected_Background}";
    std::string r;
    // rows: the options list's own row, taller
    r += "            <Style x:Key=\"Ach_Item_Section\" TargetType=\"{x:Type ListBoxItem}\" BasedOn=\"{StaticResource A_Style_ListBoxItem_List_Default}\">\r\n"
         "                <Setter Property=\"Height\" Value=\"72\" />\r\n"
         "            </Style>\r\n"
         "            <Style x:Key=\"Ach_Item_Card\" TargetType=\"{x:Type ListBoxItem}\" BasedOn=\"{StaticResource A_Style_ListBoxItem_List_Default}\">\r\n"
         "                <Setter Property=\"Height\" Value=\"124\" />\r\n"
         "            </Style>\r\n"
         // a button that's only its content
         "            <ControlTemplate x:Key=\"Ach_Plain\" TargetType=\"{x:Type Button}\">\r\n"
         "                <ContentPresenter />\r\n"
         "            </ControlTemplate>\r\n";
    r += text_style("Ach_Text_Section", "A_Style_FZTextBlock_Heading_ExtraSmall", "#FF212121", true);
    r += text_style("Ach_Text_SectionCount", "A_Style_FZTextBlock_Heading_Minimum", "#FF8C8C8C");
    r += text_style("Ach_Text_Name", "A_Style_FZTextBlock_Heading_Small", "#FF212121", true);
    r += text_style("Ach_Text_Name_Locked", "A_Style_FZTextBlock_Heading_Small", "#FF7A7A7A", true);
    r += text_style("Ach_Text_Desc", "A_Style_FZTextBlock_Body", "#FF4A4A4A");
    r += text_style("Ach_Text_Desc_Locked", "A_Style_FZTextBlock_Body", "#FF8C8C8C");
    r += text_style("Ach_Text_Status", "A_Style_FZTextBlock_Heading_Minimum", "#FF8C8C8C");
    r += text_style("Ach_Text_Status_Done", "A_Style_FZTextBlock_Heading_Minimum", orange);
    // ONLINE ONLY: salmon, to stand out from the grey without looking like the unlocked orange
    r += text_style("Ach_Text_Status_Online", "A_Style_FZTextBlock_Heading_Minimum", "#FFFA8072");
    r += text_style("Ach_Text_Score", "A_Style_FZTextBlock_Heading_Small", "#FF212121");
    r += text_style("Ach_Text_Score_Locked", "A_Style_FZTextBlock_Heading_Small", "#FFA0A0A0");
    r += material_style("Ach_Bar_Track", "#FFD9D9D9", "#66FFFFFF");
    r += material_style("Ach_Bar_Fill", orange, white);
    r += material_style("Ach_Icon_Unlocked", "#FFFFFFFF", "#FFFFFFFF",
                        "                <Setter Property=\"ImageSource\" Value=\"/ForzaUI;component/Textures/Anthem/Icons/Trophy_Icon_Medium.png\" />\r\n"
                        "                <Setter Property=\"Width\" Value=\"64\" />\r\n"
                        "                <Setter Property=\"Height\" Value=\"64\" />\r\n");
    r += material_style("Ach_Icon_Locked", "#FFB4B4B4", white,
                        "                <Setter Property=\"ImageSource\" Value=\"/ForzaUI;component/Textures/Anthem/Icons/PauseMenu/Lock_Large.png\" />\r\n"
                        "                <Setter Property=\"Width\" Value=\"38\" />\r\n"
                        "                <Setter Property=\"Height\" Value=\"50\" />\r\n");
    // the header over the cards, and the summary by the title
    r += "            <Style x:Key=\"Ach_Text_Header\" TargetType=\"local:FZTextBlock\" BasedOn=\"{StaticResource A_Style_FZTextBlock_Heading_Small}\">\r\n"
         "                <Setter Property=\"Color\" Value=\"{StaticResource A_Color_TileSelected_Text}\" />\r\n"
         "                <Setter Property=\"VerticalAlignment\" Value=\"Center\" />\r\n"
         "            </Style>\r\n"
         "            <Style x:Key=\"Ach_Text_HeaderDim\" TargetType=\"local:FZTextBlock\" BasedOn=\"{StaticResource A_Style_FZTextBlock_Heading_Minimum}\">\r\n"
         "                <Setter Property=\"Color\" Value=\"#FFB4B4B4\" />\r\n"
         "                <Setter Property=\"VerticalAlignment\" Value=\"Center\" />\r\n"
         "            </Style>\r\n";
    return r;
}

// a progress bar `width` wide, `height` high, filled `frac`
std::string bar(double width, double height, double frac, const char* margin = "0") {
    frac = std::clamp(frac, 0.0, 1.0);
    std::string x = "<Grid Width=\"" + fmt("%.0f", width) + "\" Height=\"" + fmt("%.0f", height) + "\" Margin=\"" + margin +
                    "\" VerticalAlignment=\"Center\" HorizontalAlignment=\"Left\">"
                    "<local:FZMaterial Style=\"{StaticResource Ach_Bar_Track}\" />";
    if (frac > 0)
        x += "<local:FZMaterial Style=\"{StaticResource Ach_Bar_Fill}\" HorizontalAlignment=\"Left\" Width=\"" + fmt("%.1f", width * frac) +
             "\" />";
    return x + "</Grid>";
}

std::string card_xaml(const Card& c, const std::string& name, const std::map<std::string, std::string>& unlocked) {
    const Def& d   = *c.d;
    const bool got = c.order == kUnlocked;
    std::string status, status_style = "Ach_Text_Status";
    bool show_bar = false;
    switch (c.order) {
        case kUnlocked: {
            const std::string& at = unlocked.at(d.id);
            status       = "UNLOCKED" + (at.size() >= 10 ? " " + at.substr(0, 10) : "");
            status_style = "Ach_Text_Status_Done";
            break;
        }
        case kLocked:
            if (c.measured) {
                show_bar = true;
                status   = number(c.p.have) + " / " + number(c.p.need) + " " + d.unit;
            } else {
                status = "LOCKED";
            }
            break;
        case kOnline:
            status       = "ONLINE ONLY";
            status_style = "Ach_Text_Status_Online";
            break;
        case kPending: status = "LOCKED"; break;
    }
    std::string x;
    x += "                                    <Button x:Name=\"" + name + "\" Template=\"{StaticResource Ach_Plain}\">\r\n";
    x += "                                        <Grid Height=\"124\">\r\n"
         "                                            <Grid.ColumnDefinitions><ColumnDefinition Width=\"76\" /><ColumnDefinition /><ColumnDefinition Width=\"96\" /></Grid.ColumnDefinitions>\r\n";
    x += std::string("                                            <local:FZMaterial Style=\"{StaticResource ") + (got ? "Ach_Icon_Unlocked" : "Ach_Icon_Locked") +
         "}\" HorizontalAlignment=\"Left\" VerticalAlignment=\"Center\" />\r\n";
    x += "                                            <StackPanel Grid.Column=\"1\" VerticalAlignment=\"Center\" Margin=\"0,0,12,0\">\r\n";
    x += "                                                <local:FZTextBlock Text=\"" + esc(d.name) + "\" Style=\"{StaticResource " +
         (got ? "Ach_Text_Name" : "Ach_Text_Name_Locked") + "}\" HorizontalAlignment=\"Left\" />\r\n";
    x += "                                                <local:FZTextBlock Text=\"" + esc(d.desc) + "\" Style=\"{StaticResource " +
         (got ? "Ach_Text_Desc" : "Ach_Text_Desc_Locked") + "}\" TextWrapping=\"Wrap\" HorizontalAlignment=\"Left\" Margin=\"0,2,0,0\" />\r\n";
    x += "                                                <StackPanel Orientation=\"Horizontal\" Margin=\"0,6,0,0\">";
    if (show_bar) x += bar(220, 8, c.p.need > 0 ? c.p.have / c.p.need : 0, "0,0,12,0");
    x += "<local:FZTextBlock Text=\"" + esc(status) + "\" Style=\"{StaticResource " + status_style + "}\" VerticalAlignment=\"Center\" /></StackPanel>\r\n";
    x += "                                            </StackPanel>\r\n";
    x += "                                            <local:FZTextBlock Grid.Column=\"2\" Text=\"" + gs(d.gamerscore) + "\" Style=\"{StaticResource " +
         (got ? "Ach_Text_Score" : "Ach_Text_Score_Locked") + "}\" HorizontalAlignment=\"Right\" VerticalAlignment=\"Center\" />\r\n";
    x += "                                        </Grid>\r\n"
         "                                    </Button>\r\n";
    return x;
}

// a card that isn't an achievement (nothing unlocked yet)
std::string note_card(const std::string& name, const std::string& title, const std::string& text) {
    return "                                    <Button x:Name=\"" + name + "\" Template=\"{StaticResource Ach_Plain}\">\r\n"
           "                                        <StackPanel Height=\"124\" VerticalAlignment=\"Center\" Margin=\"76,34,12,0\">\r\n"
           "                                            <local:FZTextBlock Text=\"" + esc(title) + "\" Style=\"{StaticResource Ach_Text_Name_Locked}\" HorizontalAlignment=\"Left\" />\r\n"
           "                                            <local:FZTextBlock Text=\"" + esc(text) + "\" Style=\"{StaticResource Ach_Text_Desc_Locked}\" TextWrapping=\"Wrap\" HorizontalAlignment=\"Left\" />\r\n"
           "                                        </StackPanel>\r\n"
           "                                    </Button>\r\n";
}

std::string list_open(const std::string& name, const std::string& item_style, double width, double height, const std::string& extra = "") {
    return "                                <local:CommonListBox x:Name=\"" + name + "\"" + extra +
           " KeyboardNavigation.DirectionalNavigation=\"Contained\" Style=\"{StaticResource A_Style_ListBox_List_Default}\""
           " ItemContainerStyle=\"{StaticResource " + item_style + "}\" Width=\"" + fmt("%.0f", width) + "\" Height=\"" + fmt("%.0f", height) +
           "\" HorizontalAlignment=\"Stretch\" VerticalAlignment=\"Stretch\">\r\n";
}

}  // namespace

std::vector<std::string> section_titles() {
    std::vector<std::string> t;
    for (const SectionDef& s : section_defs()) t.push_back(s.title);
    return t;
}

std::string section_check() {
    // every achievement in exactly one section (the recent ones aside)
    std::map<std::string, int> seen;
    for (const SectionDef& s : section_defs()) {
        for (const std::string& id : s.ids) {
            if (!find_def(id)) return "no achievement called " + id;
            ++seen[id];
        }
        if (s.group >= 0)
            for (const Def& d : defs())
                if (static_cast<int>(d.group) == s.group) ++seen[d.id];
    }
    for (const Def& d : defs())
        if (seen[d.id] != 1) return std::string(d.id) + " is in " + std::to_string(seen[d.id]) + " sections";
    return "";
}

PageXaml page_xaml(const Facts* f, const std::map<std::string, std::string>& unlocked) {
    constexpr double kLeftW = 400, kRightW = 820, kH = 600, kHeaderH = 66;
    int got = 0, all = 0, score = 0, scoreAll = 0;
    for (const Def& d : defs()) {
        ++all;
        scoreAll += d.gamerscore;
        if (unlocked.count(d.id)) {
            ++got;
            score += d.gamerscore;
        }
    }

    // each section's cards: unlocked first (newest first in RECENTLY UNLOCKED), then the locked
    // ones closest to unlocking, then online only, then not tracked
    const auto& secs = section_defs();
    std::vector<std::vector<Card>> cards(secs.size());
    for (size_t k = 0; k < secs.size(); ++k) {
        if (k == 0) {
            std::vector<std::pair<std::string, const Def*>> recent;
            for (const auto& [id, when] : unlocked)
                if (const Def* d = find_def(id)) recent.push_back({when, d});
            std::sort(recent.begin(), recent.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
            for (size_t i = 0; i < recent.size() && i < kRecent; ++i) cards[k].push_back(card_for(*recent[i].second, f, unlocked));
            continue;
        }
        for (const Def& d : defs()) {
            const bool in = secs[k].group >= 0 ? static_cast<int>(d.group) == secs[k].group
                                               : std::find(secs[k].ids.begin(), secs[k].ids.end(), d.id) != secs[k].ids.end();
            if (in) cards[k].push_back(card_for(d, f, unlocked));
        }
        std::stable_sort(cards[k].begin(), cards[k].end(), [](const Card& a, const Card& b) {
            if (a.order != b.order) return a.order < b.order;
            if (a.order != kLocked) return false;
            const double fa = a.measured && a.p.need > 0 ? a.p.have / a.p.need : -1;
            const double fb = b.measured && b.p.need > 0 ? b.p.have / b.p.need : -1;
            return fa > fb;
        });
    }

    PageXaml x;
    x.resources = resources();

    std::string& c = x.content;
    c += "<StackPanel Orientation=\"Vertical\" VerticalAlignment=\"Center\" HorizontalAlignment=\"Center\">\r\n";
    // the title, and the summary on the right in a block like it
    c += "                    <Grid Width=\"" + fmt("%.0f", kLeftW + 6 + kRightW) + "\" Margin=\"0,0,0,6\" Style=\"{StaticResource A_Style_FrameworkElement_DefaultTransition_0}\">\r\n"
         "                        <ContentControl Content=\"ACHIEVEMENTS\" Style=\"{StaticResource A_Style_ContentControl_ScreenTitleBlock}\" Focusable=\"False\" HorizontalAlignment=\"Left\" />\r\n"
         "                        <ContentControl Template=\"{StaticResource A_ControlTemplate_ContentControl_ScreenTitleBlock}\" Focusable=\"False\" HorizontalAlignment=\"Right\">\r\n"
         "                            <StackPanel Orientation=\"Horizontal\">\r\n"
         "                                <local:FZMaterial ImageSource=\"/ForzaUI;component/Textures/Anthem/Icons/Trophy_Icon_Medium.png\" Width=\"60\" Height=\"60\" Margin=\"-10,-10,4,-10\" VerticalAlignment=\"Center\" />\r\n"
         "                                <local:FZTextBlock Text=\"" + std::to_string(got) + " / " + std::to_string(all) + "\" Style=\"{StaticResource A_Style_FZTextBlock_ScreenTitleBlockText}\" VerticalAlignment=\"Center\" />\r\n"
         "                                <local:FZTextBlock Text=\"" + std::to_string(score) + " / " + gs(scoreAll) + "\" Style=\"{StaticResource A_Style_FZTextBlock_ScreenTitleBlockText_Inactive}\" VerticalAlignment=\"Center\" Margin=\"24,0,0,0\" />\r\n"
         "                            </StackPanel>\r\n"
         "                        </ContentControl>\r\n"
         "                    </Grid>\r\n";
    c += "                    <StackPanel Orientation=\"Horizontal\" Style=\"{StaticResource A_Style_FrameworkElement_DefaultTransition_0}\">\r\n";
    // left: the sections
    c += "                        <Grid Margin=\"0,0,6,0\" local:UIPageTransitionManager.TransitionIndex=\"0\">\r\n"
         "                            <local:FZMaterial Color=\"{StaticResource A_Color_TileSelected_Text}\">\r\n" +
         // While the focus is on the cards, the sections are switched off, so they can't take it:
         // the page puts the focus back on OptionsList (its FocusManager.FocusedElement) when it's
         // elsewhere, if it can. (On MODS > Installed mods, OptionsList is hidden at such times.)
         list_open("OptionsList", "Ach_Item_Section", kLeftW, kH,
                   " SelectedIndex=\"{Binding FH2ML_Slot_" + std::to_string(kSlotSelected) + ", Mode=TwoWay}\" IsEnabled=\"{Binding FH2ML_Slot_" +
                       std::to_string(kSlotPane) + "_Is_0}\"");
    for (size_t k = 0; k < secs.size(); ++k) {
        int sGot = 0, sAll = 0;
        for (const Card& cd : cards[k]) {
            ++sAll;
            sGot += cd.order == kUnlocked;
        }
        const std::string name  = k == 0 ? "FirstItem" : "Ach_Sec_" + std::to_string(k);
        const std::string count = k == 0 ? std::to_string(sAll) : std::to_string(sGot) + " / " + std::to_string(sAll);
        c += "                                    <Button x:Name=\"" + name + "\" Template=\"{StaticResource Ach_Plain}\">\r\n"
             "                                        <Grid Height=\"72\">\r\n"
             // the selected section, while the focus is on its cards: a bar down the left
             "                                            <local:FZMaterial Width=\"8\" HorizontalAlignment=\"Left\" Margin=\"-30,0,0,0\" Color=\"{StaticResource A_Color_TileSelected_Background}\""
             " Visibility=\"{Binding FH2ML_Slot_" + std::to_string(kSlotSection) + "_Is_" + std::to_string(k) + ", Converter={StaticResource FH2ML_ShowIf}}\" />\r\n"
             "                                            <StackPanel VerticalAlignment=\"Center\">\r\n"
             "                                                <Grid>\r\n"
             "                                                    <Grid.ColumnDefinitions><ColumnDefinition /><ColumnDefinition Width=\"Auto\" /></Grid.ColumnDefinitions>\r\n"
             "                                                    <local:FZTextBlock Text=\"" + esc(secs[k].title) + "\" Style=\"{StaticResource Ach_Text_Section}\" HorizontalAlignment=\"Left\" Margin=\"0,0,12,0\" />\r\n"
             "                                                    <local:FZTextBlock Grid.Column=\"1\" Text=\"" + count + "\" Style=\"{StaticResource Ach_Text_SectionCount}\" HorizontalAlignment=\"Right\" VerticalAlignment=\"Center\" />\r\n"
             "                                                </Grid>\r\n";
        if (k > 0) c += "                                                " + bar(kLeftW - 60, 4, sAll ? double(sGot) / sAll : 0, "0,8,0,0") + "\r\n";
        c += "                                            </StackPanel>\r\n"
             "                                        </Grid>\r\n"
             "                                    </Button>\r\n";
    }
    c += "                                </local:CommonListBox>\r\n"
         "                            </local:FZMaterial>\r\n"
         "                        </Grid>\r\n";
    // right: each section's header and cards, only the selected one's showing
    c += "                        <Grid local:UIPageTransitionManager.TransitionIndex=\"1\">\r\n";
    for (size_t k = 0; k < secs.size(); ++k) {
        int sGot = 0, sScore = 0, sScoreAll = 0;
        for (const Card& cd : cards[k]) {
            sScoreAll += cd.d->gamerscore;
            if (cd.order == kUnlocked) {
                ++sGot;
                sScore += cd.d->gamerscore;
            }
        }
        const std::string stats = k == 0 ? "THE LAST " + std::to_string(kRecent) + " YOU UNLOCKED"
                                         : std::to_string(sGot) + " / " + std::to_string(cards[k].size()) + "    " + std::to_string(sScore) + " / " + gs(sScoreAll);
        c += "                            <StackPanel Visibility=\"{Binding FH2ML_Slot_" + std::to_string(kSlotSection) + "_Is_" + std::to_string(k) +
             ", Converter={StaticResource FH2ML_ShowIfHidden}}\">\r\n"
             "                                <local:FZMaterial Color=\"{StaticResource A_Color_DarkPanelHeader}\" Width=\"" + fmt("%.0f", kRightW) + "\" Height=\"" + fmt("%.0f", kHeaderH) + "\">\r\n"
             "                                    <Grid Margin=\"30,0,30,0\">\r\n"
             "                                        <local:FZTextBlock Text=\"" + esc(secs[k].title) + "\" Style=\"{StaticResource Ach_Text_Header}\" HorizontalAlignment=\"Left\" />\r\n"
             "                                        <local:FZTextBlock Text=\"" + esc(stats) + "\" Style=\"{StaticResource Ach_Text_HeaderDim}\" HorizontalAlignment=\"Right\" />\r\n"
             "                                    </Grid>\r\n"
             "                                </local:FZMaterial>\r\n"
             "                                <local:FZMaterial Color=\"{StaticResource A_Color_TileSelected_Text}\">\r\n" +
             list_open("Ach_Cards_" + std::to_string(k), "Ach_Item_Card", kRightW, kH - kHeaderH);
        if (cards[k].empty())
            c += note_card("Ach_Card_" + std::to_string(k) + "_0", "Nothing unlocked yet", "The achievements you unlock show here, newest first.");
        for (size_t i = 0; i < cards[k].size(); ++i)
            c += card_xaml(cards[k][i], "Ach_Card_" + std::to_string(k) + "_" + std::to_string(i), unlocked);
        c += "                                </local:CommonListBox>\r\n"
             "                                </local:FZMaterial>\r\n"
             "                            </StackPanel>\r\n";
    }
    c += "                        </Grid>\r\n"
         "                    </StackPanel>\r\n"
         "                </StackPanel>";

    // the help bar: on the sections, A goes into them; on the cards, B comes back
    const std::string pane = "FH2ML_Slot_" + std::to_string(kSlotPane);
    // LB and RB: the section before and after (achievements.cpp)
    const std::string bumpers = "                    local:ControllerButtonPanel.LBumperDescription=\"{local:Loc HelpButtons.IDS_Previous}\"\r\n"
                                "                    local:ControllerButtonPanel.RBumperDescription=\"{local:Loc HelpButtons.IDS_Next}\"\r\n";
    x.help = "<ContentControl x:Name=\"DefaultHelpPanel\" Visibility=\"{Binding " + pane + "_Is_0, Converter={StaticResource FH2ML_ShowIf}}\"\r\n"
             "                    local:ControllerButtonPanel.ADescription=\"{local:Loc HelpButtons.IDS_Select}\"\r\n"
             "                    local:ControllerButtonPanel.BDescription=\"{local:Loc HelpButtons.IDS_Back}\"\r\n" +
             bumpers +
             "                    Style=\"{StaticResource A_Style_ContentControl_DefaultButtonBar}\" />\r\n"
             "<ContentControl x:Name=\"Ach_HelpCards\" Visibility=\"{Binding " + pane + "_Is_1, Converter={StaticResource FH2ML_ShowIf}}\"\r\n"
             "                    local:ControllerButtonPanel.BDescription=\"{local:Loc HelpButtons.IDS_Back}\"\r\n" +
             bumpers +
             "                    Style=\"{StaticResource A_Style_ContentControl_DefaultButtonBar}\" />";
    // without the indentation (a third of it): less for the game to read each time it opens
    for (std::string* part : {&x.resources, &x.content, &x.help}) {
        std::string t;
        t.reserve(part->size());
        bool line_start = true;
        for (char ch : *part) {
            if (line_start && ch == ' ') continue;
            line_start = ch == '\n';
            t += ch;
        }
        *part = std::move(t);
    }
    return x;
}

}  // namespace ach
