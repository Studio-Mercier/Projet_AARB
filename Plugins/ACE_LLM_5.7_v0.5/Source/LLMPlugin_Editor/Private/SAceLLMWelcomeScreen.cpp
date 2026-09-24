#include "SAceLLMWelcomeScreen.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/AppStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAceLLMWelcomeScreen"

// ── Config keys ──────────────────────────────────────────────────────────────

static const TCHAR* ConfigSection          = TEXT("NVIDIA_ACE_LLM");
static const TCHAR* ConfigKeyShowOnStartup = TEXT("ShowWelcomeScreenOnStartup");

// ── Outbound URLs ────────────────────────────────────────────────────────────

static const TCHAR* URL_Feedback = TEXT("https://discord.com/channels/1019361803752456192/1256019798920527992");
static const TCHAR* URL_Forums   = TEXT("https://developer.nvidia.com/ace-for-games");

// ── Static state ─────────────────────────────────────────────────────────────

TWeakPtr<SWindow> SAceLLMWelcomeScreen::ActiveWindowWeakPtr;

// ── Config helpers ───────────────────────────────────────────────────────────

bool SAceLLMWelcomeScreen::ShouldShowOnStartup()
{
	bool bShow = true;
	if (GConfig)
	{
		GConfig->GetBool(ConfigSection, ConfigKeyShowOnStartup, bShow, GEditorPerProjectIni);
	}
	return bShow;
}

void SAceLLMWelcomeScreen::SetShowOnStartup(bool bShow)
{
	if (!GConfig) return;
	GConfig->SetBool(ConfigSection, ConfigKeyShowOnStartup, bShow, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

// ── Open ─────────────────────────────────────────────────────────────────────

void SAceLLMWelcomeScreen::Open()
{
	if (ActiveWindowWeakPtr.IsValid())
	{
		if (TSharedPtr<SWindow> Existing = ActiveWindowWeakPtr.Pin())
		{
			Existing->BringToFront();
			return;
		}
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WelcomeWindowTitle", "Welcome to NVIDIA ACE LLM"))
		.ClientSize(FVector2D(600.f, 460.f))
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.IsInitiallyMaximized(false);

	TSharedRef<SAceLLMWelcomeScreen> Content =
		SNew(SAceLLMWelcomeScreen).ParentWindowWeakPtr(Window);

	Window->SetContent(Content);
	FSlateApplication::Get().AddWindow(Window);

	ActiveWindowWeakPtr = Window;
}

// ── Construct ────────────────────────────────────────────────────────────────

void SAceLLMWelcomeScreen::Construct(const FArguments& InArgs)
{
	ParentWindowWeakPtr = InArgs._ParentWindowWeakPtr;

	const FMargin ButtonPadding(14.f, 7.f);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(0.f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			// ── Header ───────────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(FMargin(24.f, 20.f))
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 5.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Header_Title", "NVIDIA ACE LLM"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Header_Subtitle",
							"Local large language model inference for Unreal Engine \u2014 powered by NVIDIA"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]

			// ── Body ─────────────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(24.f, 20.f, 24.f, 16.f)
			[
				SNew(SScrollBox)

				+ SScrollBox::Slot()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 18.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Body_Welcome",
							"Thank you for installing NVIDIA ACE LLM!\n\n"
							"This plugin brings real-time, local large language model inference into Unreal Engine "
							"using the NVIDIA Game Agent SDK Chat APIs. Drop the ACE LLM component onto an NPC for "
							"Blueprint-friendly conversational and agentic behavior \u2014 chat history, streaming token "
							"output, and tool calling are all included, with no prompts ever leaving the machine.\n\n"
							"We would love to hear your feedback and feature requests, or issues in our Discord channel. "
							"Please include [ACE LLM] in your message so we can find it easily!"))
						.AutoWrapText(true)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 16.f)
					[
						SNew(SSeparator).Orientation(Orient_Horizontal)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 10.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Links_Header", "Get Involved"))
					]

					// ── Link buttons ─────────────────────────────────────────
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 8.f, 0.f)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
							.ContentPadding(ButtonPadding)
							.ToolTipText(LOCTEXT("Feedback_Tip", "Open the ACE LLM Discord feedback channel"))
							.OnClicked(this, &SAceLLMWelcomeScreen::OnFeedbackClicked)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Btn_Feedback", "Connect on Discord"))
							]
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.ContentPadding(ButtonPadding)
							.ToolTipText(LOCTEXT("Forums_Tip", "Visit the NVIDIA ACE for Games developer portal"))
							.OnClicked(this, &SAceLLMWelcomeScreen::OnForumsClicked)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Btn_Forums", "Developer Portal"))
							]
						]
					]
				]
			]

			// ── Footer ───────────────────────────────────────────────────────
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(FMargin(24.f, 12.f))
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.FillWidth(1.f)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SAceLLMWelcomeScreen::GetShowOnStartupCheckState)
						.OnCheckStateChanged(this, &SAceLLMWelcomeScreen::OnShowOnStartupCheckStateChanged)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Checkbox_ShowOnStartup", "Show on editor startup"))
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
						.ContentPadding(FMargin(20.f, 7.f))
						.OnClicked(this, &SAceLLMWelcomeScreen::OnCloseClicked)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Btn_Close", "Close"))
						]
					]
				]
			]
		]
	];
}

// ── Button callbacks ─────────────────────────────────────────────────────────

FReply SAceLLMWelcomeScreen::OnFeedbackClicked()
{
	FPlatformProcess::LaunchURL(URL_Feedback, nullptr, nullptr);
	return FReply::Handled();
}

FReply SAceLLMWelcomeScreen::OnForumsClicked()
{
	FPlatformProcess::LaunchURL(URL_Forums, nullptr, nullptr);
	return FReply::Handled();
}

FReply SAceLLMWelcomeScreen::OnCloseClicked()
{
	if (TSharedPtr<SWindow> Window = ParentWindowWeakPtr.Pin())
	{
		Window->RequestDestroyWindow();
	}
	return FReply::Handled();
}

// ── Checkbox callbacks ───────────────────────────────────────────────────────

ECheckBoxState SAceLLMWelcomeScreen::GetShowOnStartupCheckState() const
{
	return ShouldShowOnStartup() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAceLLMWelcomeScreen::OnShowOnStartupCheckStateChanged(ECheckBoxState NewState)
{
	SetShowOnStartup(NewState == ECheckBoxState::Checked);
}

#undef LOCTEXT_NAMESPACE
