#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SWindow;

/**
 * Slate welcome / feedback popup for the NVIDIA ACE LLM plugin.
 *
 * Shown automatically on editor startup when the preference is enabled
 * (default: true on first install).  Also reachable at any time via
 * Tools > NVIDIA ACE LLM > Provide ACE Feedback.
 *
 * Contains two outbound link buttons (Discord, Developer Portal) and a
 * checkbox to control auto-show on startup.  The preference is persisted
 * to EditorPerProjectUserSettings.ini under [NVIDIA_ACE_LLM].
 */
class SAceLLMWelcomeScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAceLLMWelcomeScreen) {}
		SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindowWeakPtr)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Opens the welcome window (or brings the existing one to front). */
	static void Open();

	/** Returns true when auto-show on startup is enabled. */
	static bool ShouldShowOnStartup();

	/** Writes the auto-show preference and flushes immediately. */
	static void SetShowOnStartup(bool bShow);

private:
	TWeakPtr<SWindow> ParentWindowWeakPtr;
	static TWeakPtr<SWindow> ActiveWindowWeakPtr;

	FReply OnFeedbackClicked();
	FReply OnForumsClicked();
	FReply OnCloseClicked();

	ECheckBoxState GetShowOnStartupCheckState() const;
	void           OnShowOnStartupCheckStateChanged(ECheckBoxState NewState);
};
