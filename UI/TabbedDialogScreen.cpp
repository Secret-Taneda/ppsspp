#include <algorithm>

#include "Common/StringUtils.h"
#include "Common/System/NativeApp.h"
#include "Common/System/Request.h"
#include "Common/System/Display.h"
#include "UI/TabbedDialogScreen.h"

UI::LinearLayout *TabbedUIDialogScreenWithGameBackground::AddTab(const char *tag, const std::string &title, bool isSearch) {
	auto se = GetI18NCategory(I18NCat::SEARCH);

	using namespace UI;
	ViewGroup *scroll = new ScrollView(ORIENT_VERTICAL, new LinearLayoutParams(FILL_PARENT, FILL_PARENT));
	scroll->SetTag(tag);

	LinearLayout *contents = new LinearLayoutList(ORIENT_VERTICAL);
	contents->SetSpacing(0);
	scroll->Add(contents);
	tabHolder_->AddTab(title, scroll);

	if (!isSearch) {
		settingTabContents_.push_back(contents);

		auto notice = contents->Add(new TextView(se->T("Filtering settings by '%1'"), new LinearLayoutParams(Margins(20, 5))));
		notice->SetVisibility(V_GONE);
		settingTabFilterNotices_.push_back(notice);
	}

	return contents;
}

void TabbedUIDialogScreenWithGameBackground::CreateViews() {
	PreCreateViews();

	bool vertical = UseVerticalLayout();

	// Information in the top left.
	// Back button to the bottom left.
	// Scrolling action menu to the right.
	using namespace UI;

	auto ms = GetI18NCategory(I18NCat::MAINSETTINGS);
	auto di = GetI18NCategory(I18NCat::DIALOG);

	root_ = new AnchorLayout(new LayoutParams(FILL_PARENT, FILL_PARENT));

	if (vertical) {
		LinearLayout *verticalLayout = new LinearLayout(ORIENT_VERTICAL, new LayoutParams(FILL_PARENT, FILL_PARENT));
		tabHolder_ = new TabHolder(ORIENT_HORIZONTAL, 200, new LinearLayoutParams(1.0f));
		verticalLayout->Add(tabHolder_);
		verticalLayout->Add(new Choice(di->T("Back"), "", false, new LinearLayoutParams(FILL_PARENT, WRAP_CONTENT, 0.0f, Margins(0))))->OnClick.Handle<UIScreen>(this, &UIScreen::OnBack);
		root_->Add(verticalLayout);
	} else {
		tabHolder_ = new TabHolder(ORIENT_VERTICAL, 200, new AnchorLayoutParams(10, 0, 10, 0, false));
		root_->Add(tabHolder_);
		AddStandardBack(root_);
	}
	tabHolder_->SetTag(tag());  // take the tag from the screen.
	root_->SetDefaultFocusView(tabHolder_);
	settingTabContents_.clear();
	settingTabFilterNotices_.clear();

	float leftSide = 40.0f;
	if (!vertical) {
		leftSide += 200.0f;
	}
	settingInfo_ = new SettingInfoMessage(ALIGN_CENTER | FLAG_WRAP_TEXT, new AnchorLayoutParams(
		g_display.dp_xres - leftSide - 40.0f, WRAP_CONTENT,
		leftSide, g_display.dp_yres - 80.0f - 40.0f, NONE, NONE));
	settingInfo_->SetBottomCutoff(g_display.dp_yres - 200.0f);
	root_->Add(settingInfo_);

	// Show it again if we recreated the view
	if (!oldSettingInfo_.empty()) {
		settingInfo_->Show(oldSettingInfo_, nullptr);
	}

	// Let the subclass create its tabs.
	CreateTabs();

#if !defined(MOBILE_DEVICE) || PPSSPP_PLATFORM(ANDROID)
	// Hide search if screen is too small.
	int deviceType = System_GetPropertyInt(SYSPROP_DEVICE_TYPE);
	if ((g_display.dp_xres < g_display.dp_yres || g_display.dp_yres >= 500) && (deviceType != DEVICE_TYPE_VR) && ShowSearchControls()) {
		auto se = GetI18NCategory(I18NCat::SEARCH);
		// Search
		LinearLayout *searchSettings = AddTab("GameSettingsSearch", ms->T("Search"), true);

		searchSettings->Add(new ItemHeader(se->T("Find settings")));
		if (System_GetPropertyBool(SYSPROP_HAS_KEYBOARD)) {
			searchSettings->Add(new ChoiceWithValueDisplay(&searchFilter_, se->T("Filter"), I18NCat::NONE))->OnClick.Handle(this, &TabbedUIDialogScreenWithGameBackground::OnChangeSearchFilter);
		} else {
			searchSettings->Add(new PopupTextInputChoice(&searchFilter_, se->T("Filter"), "", 64, screenManager()))->OnChange.Handle(this, &TabbedUIDialogScreenWithGameBackground::OnChangeSearchFilter);
		}
		clearSearchChoice_ = searchSettings->Add(new Choice(se->T("Clear filter")));
		clearSearchChoice_->OnClick.Handle(this, &TabbedUIDialogScreenWithGameBackground::OnClearSearchFilter);
		noSearchResults_ = searchSettings->Add(new TextView(se->T("No settings matched '%1'"), new LinearLayoutParams(Margins(20, 5))));

		ApplySearchFilter();
	}
#endif
}

void TabbedUIDialogScreenWithGameBackground::sendMessage(const char *message, const char *value) {
	UIDialogScreenWithGameBackground::sendMessage(message, value);
	if (!strcmp(message, "gameSettings_search")) {
		std::string filter = value ? value : "";
		searchFilter_.resize(filter.size());
		std::transform(filter.begin(), filter.end(), searchFilter_.begin(), tolower);

		ApplySearchFilter();
	}
}

void TabbedUIDialogScreenWithGameBackground::RecreateViews() {
	oldSettingInfo_ = settingInfo_ ? settingInfo_->GetText() : "N/A";
	UIScreen::RecreateViews();
}

void TabbedUIDialogScreenWithGameBackground::ApplySearchFilter() {
	auto se = GetI18NCategory(I18NCat::SEARCH);

	bool matches = searchFilter_.empty();
	for (int t = 0; t < (int)settingTabContents_.size(); ++t) {
		auto tabContents = settingTabContents_[t];
		bool tabMatches = searchFilter_.empty();

		// Show an indicator that a filter is applied.
		settingTabFilterNotices_[t]->SetVisibility(tabMatches ? UI::V_GONE : UI::V_VISIBLE);
		settingTabFilterNotices_[t]->SetText(ReplaceAll(se->T("Filtering settings by '%1'"), "%1", searchFilter_));

		UI::View *lastHeading = nullptr;
		for (int i = 1; i < tabContents->GetNumSubviews(); ++i) {
			UI::View *v = tabContents->GetViewByIndex(i);
			if (!v->CanBeFocused()) {
				lastHeading = v;
			}

			std::string label = v->DescribeText();
			std::transform(label.begin(), label.end(), label.begin(), tolower);
			bool match = v->CanBeFocused() && label.find(searchFilter_) != label.npos;
			tabMatches = tabMatches || match;

			if (match && lastHeading)
				lastHeading->SetVisibility(UI::V_VISIBLE);
			v->SetVisibility(searchFilter_.empty() || match ? UI::V_VISIBLE : UI::V_GONE);
		}
		tabHolder_->EnableTab(t, tabMatches);
		matches = matches || tabMatches;
	}

	noSearchResults_->SetText(ReplaceAll(se->T("No settings matched '%1'"), "%1", searchFilter_));
	noSearchResults_->SetVisibility(matches ? UI::V_GONE : UI::V_VISIBLE);
	clearSearchChoice_->SetVisibility(searchFilter_.empty() ? UI::V_GONE : UI::V_VISIBLE);
}

UI::EventReturn TabbedUIDialogScreenWithGameBackground::OnChangeSearchFilter(UI::EventParams &e) {
#if PPSSPP_PLATFORM(WINDOWS) || defined(USING_QT_UI) || defined(__ANDROID__)
	auto se = GetI18NCategory(I18NCat::SEARCH);
	System_InputBoxGetString(se->T("Search term"), searchFilter_, [](const std::string &value, int) {
		NativeMessageReceived("gameSettings_search", StripSpaces(value).c_str());
		});
#else
	if (!System_GetPropertyBool(SYSPROP_HAS_KEYBOARD))
		NativeMessageReceived("gameSettings_search", StripSpaces(searchFilter_).c_str());
#endif
	return UI::EVENT_DONE;
}

UI::EventReturn TabbedUIDialogScreenWithGameBackground::OnClearSearchFilter(UI::EventParams &e) {
	NativeMessageReceived("gameSettings_search", "");
	return UI::EVENT_DONE;
}
