#include "Modules/ModuleManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "ToolMenus.h"
#include "SAceLLMWelcomeScreen.h"

#define LOCTEXT_NAMESPACE "LLMPlugin_Editor"

class FLLMPlugin_EditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(
				this, &FLLMPlugin_EditorModule::RegisterMenus));

		IMainFrameModule& MainFrame =
			FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		MainFrameCreationFinishedHandle =
			MainFrame.OnMainFrameCreationFinished().AddRaw(
				this, &FLLMPlugin_EditorModule::OnMainFrameReady);
	}

	virtual void ShutdownModule() override
	{
		if (IMainFrameModule* MF = FModuleManager::GetModulePtr<IMainFrameModule>("MainFrame"))
		{
			MF->OnMainFrameCreationFinished().Remove(MainFrameCreationFinishedHandle);
		}
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

private:
	FDelegateHandle MainFrameCreationFinishedHandle;

	void OnMainFrameReady(TSharedPtr<SWindow> /*InRootWindow*/, bool /*bIsNewProject*/)
	{
		if (SAceLLMWelcomeScreen::ShouldShowOnStartup())
		{
			SAceLLMWelcomeScreen::Open();
		}
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		if (!ToolsMenu) return;

		FToolMenuSection& Section = ToolsMenu->FindOrAddSection(
			"NvidiaACELLM",
			LOCTEXT("NvidiaACELLMSectionLabel", "NVIDIA ACE LLM"));

		Section.AddMenuEntry(
			"OpenACELLMWelcomeScreen",
			LOCTEXT("OpenWelcomeScreen", "Provide ACE Feedback"),
			LOCTEXT("OpenWelcomeScreenTooltip",
				"Open the NVIDIA ACE LLM welcome screen with feedback and documentation links."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				SAceLLMWelcomeScreen::Open();
			}))
		);
	}
};

IMPLEMENT_MODULE(FLLMPlugin_EditorModule, LLMPlugin_Editor)

#undef LOCTEXT_NAMESPACE
