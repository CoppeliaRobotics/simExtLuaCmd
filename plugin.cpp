#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "UIFunctions.h"
#include "UIProxy.h"
#include "simPlusPlus/Plugin.h"
#include "plugin.h"
#include "stubs.h"
#include "config.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QSplitter>
#include "PersistentOptions.h"
#include "qluacommanderwidget.h"

class Plugin : public sim::Plugin
{
public:
    void onStart()
    {
        menuHandles.clear();
        menuState.clear();
        menuLabels.clear();
        firstInstancePass = true;
        pluginEnabled = true;

        UIProxy::getInstance(); // construct UIProxy here (UI thread)

        // find the StatusBar widget (QPlainTextEdit)
        statusBar = UIProxy::simMainWindow->findChild<QPlainTextEdit*>("statusBar");
        if(!statusBar)
            throw std::runtime_error("cannot find the statusbar widget");

        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

        setExtVersion("Lua REPL (read-eval-print-loop) Plugin");
        setBuildDate(BUILD_DATE);

        optionsChangedFromGui.store(false);
        optionsChangedFromData.store(false);

        // attach widget to CoppeliaSim main window
        splitter = (QSplitter*)statusBar->parentWidget();
        UIProxy::getInstance()->setStatusBar(statusBar, splitter);
        splitterChild = new QWidget();
        splitter->replaceWidget(1,splitterChild);
        layout = new QVBoxLayout();
        layout->setSpacing(0);
        layout->setMargin(0);
        layout->setContentsMargins(0,0,0,0);
        splitterChild->setLayout(layout);
        commanderWidget = new QLuaCommanderWidget();
        commanderWidget->setVisible(false);
        layout->addWidget(statusBar);
        layout->addWidget(commanderWidget);
        splitterChild->setMaximumHeight(600);

        // add menu items to CoppeliaSim main window
        MENUITEM_TOGGLE_VISIBILITY = menuLabels.size();
        menuLabels.push_back("Enable");
        menuLabels.push_back("");
        MENUITEM_HISTORY_CLEAR = menuLabels.size();
        menuLabels.push_back("Clear command history");
        menuLabels.push_back("");
        MENUITEM_PRINT_ALL_RETURNED_VALUES = menuLabels.size();
        menuLabels.push_back("Print all returned values");
        MENUITEM_WARN_ABOUT_MULTIPLE_RETURNED_VALUES = menuLabels.size();
        menuLabels.push_back("Warn about multiple returned values");
        MENUITEM_STRING_ESCAPE_SPECIALS = menuLabels.size();
        menuLabels.push_back("String rendering: escape special characters");
        MENUITEM_MAP_SORT_KEYS_BY_NAME = menuLabels.size();
        menuLabels.push_back("Map/array rendering: sort keys by name");
        MENUITEM_MAP_SORT_KEYS_BY_TYPE = menuLabels.size();
        menuLabels.push_back("Map/array rendering: sort keys by type");
        MENUITEM_MAP_SHADOW_LONG_STRINGS = menuLabels.size();
        menuLabels.push_back("Map/array rendering: shadow long strings");
        MENUITEM_MAP_SHADOW_BUFFER_STRINGS = menuLabels.size();
        menuLabels.push_back("Map/array rendering: shadow buffer strings");
        MENUITEM_MAP_SHADOW_SPECIAL_STRINGS = menuLabels.size();
        menuLabels.push_back("Map/array rendering: shadow strings with special characters");
        MENUITEM_HISTORY_SKIP_REPEATED = menuLabels.size();
        menuLabels.push_back("History: skip repeated commands");
        MENUITEM_HISTORY_REMOVE_DUPS = menuLabels.size();
        menuLabels.push_back("History: remove duplicates");
        MENUITEM_SHOW_MATCHING_HISTORY = menuLabels.size();
        menuLabels.push_back("History: show matching entries (select with UP)");
        MENUITEM_DYNAMIC_COMPLETION = menuLabels.size();
        menuLabels.push_back("Dynamic completion");
        MENUITEM_AUTO_ACCEPT_COMMON_COMPLETION_PREFIX = menuLabels.size();
        menuLabels.push_back("Auto-accept common completion prefix");
        MENUITEM_RESIZE_STATUSBAR_WHEN_FOCUSED = menuLabels.size();
        menuLabels.push_back("Resize statusbar when focused");

        menuState.resize(menuLabels.size());
        menuHandles.resize(menuLabels.size());
        if(simAddModuleMenuEntry("Lua Commander", menuHandles.size(), &menuHandles[0]) == -1)
        {
            sim::addLog(sim_verbosity_errors, "failed to create menu");
            return;
        }
        updateUI();
    }

    void onEnd()
    {
        if(commanderWidget)
        {
            layout->removeWidget(statusBar);
            delete splitter->replaceWidget(1, statusBar);
        }
        UIProxy::destroyInstance();
        UI_THREAD = NULL;
    }

    void onLastInstancePass()
    {
        UIFunctions::destroyInstance();
        SIM_THREAD = NULL;
    }

    void updateMenuItems()
    {
        menuState[MENUITEM_TOGGLE_VISIBILITY] = (options.enabled ? itemChecked : 0) + itemEnabled;
        menuState[MENUITEM_HISTORY_CLEAR] = (options.enabled ? itemEnabled : 0);
        menuState[MENUITEM_PRINT_ALL_RETURNED_VALUES] = (options.enabled ? itemEnabled : 0) + (options.printAllReturnedValues ? itemChecked : 0);
        menuState[MENUITEM_WARN_ABOUT_MULTIPLE_RETURNED_VALUES] = (options.enabled ? itemEnabled : 0) + (options.warnAboutMultipleReturnedValues ? itemChecked : 0);
        menuState[MENUITEM_STRING_ESCAPE_SPECIALS] = (options.enabled ? itemEnabled : 0) + (options.stringEscapeSpecials ? itemChecked : 0);
        menuState[MENUITEM_MAP_SORT_KEYS_BY_NAME] = (options.enabled ? itemEnabled : 0) + (options.mapSortKeysByName ? itemChecked : 0);
        menuState[MENUITEM_MAP_SORT_KEYS_BY_TYPE] = (options.enabled ? itemEnabled : 0) + (options.mapSortKeysByType ? itemChecked : 0);
        menuState[MENUITEM_MAP_SHADOW_LONG_STRINGS] = (options.enabled ? itemEnabled : 0) + (options.mapShadowLongStrings ? itemChecked : 0);
        menuState[MENUITEM_MAP_SHADOW_BUFFER_STRINGS] = (options.enabled ? itemEnabled : 0) + (options.mapShadowBufferStrings ? itemChecked : 0);
        menuState[MENUITEM_MAP_SHADOW_SPECIAL_STRINGS] = (options.enabled ? itemEnabled : 0) + (options.mapShadowSpecialStrings ? itemChecked : 0);
        menuState[MENUITEM_HISTORY_SKIP_REPEATED] = (options.enabled ? itemEnabled : 0) + (options.historySkipRepeated ? itemChecked : 0);
        menuState[MENUITEM_HISTORY_REMOVE_DUPS] = (options.enabled ? itemEnabled : 0) + (options.historyRemoveDups ? itemChecked : 0);
        menuState[MENUITEM_SHOW_MATCHING_HISTORY] = (options.enabled ? itemEnabled : 0) + (options.showMatchingHistory ? itemChecked : 0);
        menuState[MENUITEM_DYNAMIC_COMPLETION] = (options.enabled ? itemEnabled : 0) + (options.autoAcceptCommonCompletionPrefix ? itemChecked : 0);
        menuState[MENUITEM_AUTO_ACCEPT_COMMON_COMPLETION_PREFIX] = (options.enabled ? itemEnabled : 0) + (options.dynamicCompletion ? itemChecked : 0);
        menuState[MENUITEM_RESIZE_STATUSBAR_WHEN_FOCUSED] = (options.enabled ? itemEnabled : 0) + (options.resizeStatusbarWhenFocused ? itemChecked : 0);

        for(int i = 0; i < menuHandles.size(); i++)
            simSetModuleMenuItemState(menuHandles[i], menuState[i], menuLabels[i].c_str());
    }

    void updateUI()
    {
        updateMenuItems();
        if(commanderWidget)
        {
            bool oldVis = commanderWidget->isVisible();

            if(!firstInstancePass)
                commanderWidget->setVisible(options.enabled);

            bool newVis = commanderWidget->isVisible();
            if(oldVis && !newVis)
            {
                // when commander is hidden, focus the statusbar
                statusBar->setFocus();
            }
            else if(!oldVis && newVis)
            {
                // when it is shown, focus it
                commanderWidget->editor_()->setFocus();
            }
        }
    }

    virtual void onMenuItemSelected(int itemHandle, int itemState)
    {
        if(itemHandle == menuHandles[MENUITEM_TOGGLE_VISIBILITY])
        {
            options.enabled = !options.enabled;
        }
        else if(itemHandle == menuHandles[MENUITEM_HISTORY_CLEAR])
        {
            UIProxy::getInstance()->clearHistory();
        }
        else if(itemHandle == menuHandles[MENUITEM_PRINT_ALL_RETURNED_VALUES])
        {
            options.printAllReturnedValues = !options.printAllReturnedValues;
        }
        else if(itemHandle == menuHandles[MENUITEM_WARN_ABOUT_MULTIPLE_RETURNED_VALUES])
        {
            options.warnAboutMultipleReturnedValues = !options.warnAboutMultipleReturnedValues;
        }
        else if(itemHandle == menuHandles[MENUITEM_STRING_ESCAPE_SPECIALS])
        {
            options.stringEscapeSpecials = !options.stringEscapeSpecials;
        }
        else if(itemHandle == menuHandles[MENUITEM_MAP_SORT_KEYS_BY_NAME])
        {
            options.mapSortKeysByName = !options.mapSortKeysByName;
        }
        else if(itemHandle == menuHandles[MENUITEM_MAP_SORT_KEYS_BY_TYPE])
        {
            options.mapSortKeysByType = !options.mapSortKeysByType;
        }
        else if(itemHandle == menuHandles[MENUITEM_MAP_SHADOW_LONG_STRINGS])
        {
            options.mapShadowLongStrings = !options.mapShadowLongStrings;
        }
        else if(itemHandle == menuHandles[MENUITEM_MAP_SHADOW_BUFFER_STRINGS])
        {
            options.mapShadowBufferStrings = !options.mapShadowBufferStrings;
        }
        else if(itemHandle == menuHandles[MENUITEM_MAP_SHADOW_SPECIAL_STRINGS])
        {
            options.mapShadowSpecialStrings = !options.mapShadowSpecialStrings;
        }
        else if(itemHandle == menuHandles[MENUITEM_HISTORY_SKIP_REPEATED])
        {
            options.historySkipRepeated = !options.historySkipRepeated;
        }
        else if(itemHandle == menuHandles[MENUITEM_HISTORY_REMOVE_DUPS])
        {
            options.historyRemoveDups = !options.historyRemoveDups;
        }
        else if(itemHandle == menuHandles[MENUITEM_SHOW_MATCHING_HISTORY])
        {
            options.showMatchingHistory = !options.showMatchingHistory;
        }
        else if(itemHandle == menuHandles[MENUITEM_DYNAMIC_COMPLETION])
        {
            options.dynamicCompletion = !options.dynamicCompletion;
        }
        else if(itemHandle == menuHandles[MENUITEM_AUTO_ACCEPT_COMMON_COMPLETION_PREFIX])
        {
            options.autoAcceptCommonCompletionPrefix = !options.autoAcceptCommonCompletionPrefix;
        }
        else if(itemHandle == menuHandles[MENUITEM_RESIZE_STATUSBAR_WHEN_FOCUSED])
        {
            options.resizeStatusbarWhenFocused = !options.resizeStatusbarWhenFocused;
        }
        else return;

        optionsChangedFromGui.store(true);
        updateUI();
        commanderWidget->setOptions(options);
    }

    virtual void onGuiPass()
    {
        if(optionsChangedFromData.load())
        {
            optionsChangedFromData.store(false);
            updateUI();
            commanderWidget->setOptions(options);
        }
    }

    void updateScriptsList()
    {
        bool simRunning = simGetSimulationState() == sim_simulation_advancing_running;
        QMap<int,QString> childScripts;
        QMap<int,QString> customizationScripts;
        int i = 0;
        while(1)
        {
            int handle = simGetObjects(i++, sim_handle_all);
            if(handle == -1) break;
            char *name_cstr = simGetObjectName(handle);
            QString name = QString::fromUtf8(name_cstr);
            simReleaseBuffer(name_cstr);
            if(simRunning)
            {
                int childScript = simGetScriptAssociatedWithObject(handle);
                if(childScript != -1)
                    childScripts[handle] = name;
            }
            int customizationScript = simGetCustomizationScriptAssociatedWithObject(handle);
            if(customizationScript != -1)
                customizationScripts[handle] = name;
        }
        UIFunctions::getInstance()->scriptListChanged(childScripts, customizationScripts, simRunning);
    }

    virtual void onInstancePass(const sim::InstancePassFlags &flags, bool first)
    {
        if(!commanderWidget) return;

        if(firstInstancePass)
        {
            firstInstancePass = false;

            int id = qRegisterMetaType< QMap<int,QString> >();

            UIFunctions::getInstance(); // construct UIFunctions here (SIM thread)
            QObject::connect(commanderWidget, &QLuaCommanderWidget::execCode, UIFunctions::getInstance(), &UIFunctions::onExecCode);
            QObject::connect(commanderWidget, &QLuaCommanderWidget::askCompletion, UIFunctions::getInstance(), &UIFunctions::onAskCompletion);
            QObject::connect(UIFunctions::getInstance(), &UIFunctions::scriptListChanged, commanderWidget, &QLuaCommanderWidget::onScriptListChanged);
            QObject::connect(UIFunctions::getInstance(), &UIFunctions::setCompletion, commanderWidget, &QLuaCommanderWidget::onSetCompletion);
            QObject::connect(commanderWidget, &QLuaCommanderWidget::askCallTip, UIFunctions::getInstance(), &UIFunctions::onAskCallTip);
            QObject::connect(UIFunctions::getInstance(), &UIFunctions::setCallTip, commanderWidget, &QLuaCommanderWidget::onSetCallTip);
            QObject::connect(UIFunctions::getInstance(), &UIFunctions::historyChanged, commanderWidget, &QLuaCommanderWidget::setHistory);
            options.load();
            UIFunctions::getInstance()->setOptions(options);
            UIFunctions::getInstance()->loadHistory();
            optionsChangedFromData.store(true);
        }

        if(commanderWidget->closeFlag.load())
        {
            commanderWidget->closeFlag.store(false);
            options.enabled = false;
            optionsChangedFromGui.store(true);
            updateUI();
        }

        if(optionsChangedFromGui.load())
        {
            optionsChangedFromGui.store(false);
            options.save();
            UIFunctions::getInstance()->setOptions(options);
            updateUI();
        }

        if(flags.objectsErased || flags.objectsCreated || flags.modelLoaded || flags.sceneLoaded || flags.undoCalled || flags.redoCalled || flags.sceneSwitched || flags.scriptCreated || flags.scriptErased || flags.simulationStarted || flags.simulationEnded)
        {
            updateScriptsList();
        }
    }

    void setEnabled(setEnabled_in *in, setEnabled_out *out)
    {
        options.enabled = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setPrintAllReturnedValues(setPrintAllReturnedValues_in *in, setPrintAllReturnedValues_out *out)
    {
        options.printAllReturnedValues = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setWarnAboutMultipleReturnedValues(setWarnAboutMultipleReturnedValues_in *in, setWarnAboutMultipleReturnedValues_out *out)
    {
        options.warnAboutMultipleReturnedValues = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setArrayMaxItemsDisplayed(setArrayMaxItemsDisplayed_in *in, setArrayMaxItemsDisplayed_out *out)
    {
        options.arrayMaxItemsDisplayed = in->n;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setStringLongLimit(setStringLongLimit_in *in, setStringLongLimit_out *out)
    {
        options.stringLongLimit = in->n;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setStringEscapeSpecials(setStringEscapeSpecials_in *in, setStringEscapeSpecials_out *out)
    {
        options.stringEscapeSpecials = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapSortKeysByName(setMapSortKeysByName_in *in, setMapSortKeysByName_out *out)
    {
        options.mapSortKeysByName = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapSortKeysByType(setMapSortKeysByType_in *in, setMapSortKeysByType_out *out)
    {
        options.mapSortKeysByType = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapShadowLongStrings(setMapShadowLongStrings_in *in, setMapShadowLongStrings_out *out)
    {
        options.mapShadowLongStrings = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapShadowBufferStrings(setMapShadowBufferStrings_in *in, setMapShadowBufferStrings_out *out)
    {
        options.mapShadowBufferStrings = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapShadowSpecialStrings(setMapShadowSpecialStrings_in *in, setMapShadowSpecialStrings_out *out)
    {
        options.mapShadowSpecialStrings = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setFloatPrecision(setFloatPrecision_in *in, setFloatPrecision_out *out)
    {
        options.floatPrecision = in->n;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapMaxDepth(setMapMaxDepth_in *in, setMapMaxDepth_out *out)
    {
        options.mapMaxDepth = in->n;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void clearHistory(clearHistory_in *in, clearHistory_out *out)
    {
        UIFunctions::getInstance()->clearHistory();
    }

    void setHistorySize(setHistorySize_in *in, setHistorySize_out *out)
    {
        options.historySize = in->n;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setHistorySkipRepeated(setHistorySkipRepeated_in *in, setHistorySkipRepeated_out *out)
    {
        options.historySkipRepeated = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setHistoryRemoveDups(setHistoryRemoveDups_in *in, setHistoryRemoveDups_out *out)
    {
        options.historyRemoveDups = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setShowMatchingHistory(setShowMatchingHistory_in *in, setShowMatchingHistory_out *out)
    {
        options.showMatchingHistory = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setDynamicCompletion(setDynamicCompletion_in *in, setDynamicCompletion_out *out)
    {
        options.dynamicCompletion = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setAutoAcceptCommonCompletionPrefix(setAutoAcceptCommonCompletionPrefix_in *in, setAutoAcceptCommonCompletionPrefix_out *out)
    {
        options.autoAcceptCommonCompletionPrefix = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setResizeStatusbarWhenFocused(setResizeStatusbarWhenFocused_in *in, setResizeStatusbarWhenFocused_out *out)
    {
        options.resizeStatusbarWhenFocused = in->b;
        options.save();
        UIFunctions::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

private:
    std::atomic<bool> optionsChangedFromGui;
    std::atomic<bool> optionsChangedFromData;
    PersistentOptions options;
    bool firstInstancePass = true;
    bool pluginEnabled = true;
    QPlainTextEdit *statusBar;
    QSplitter *splitter = 0L;
    QWidget *splitterChild = 0L;
    QVBoxLayout *layout = 0L;
    QLuaCommanderWidget *commanderWidget = 0L;
    std::vector<simInt> menuHandles;
    std::vector<simInt> menuState;
    std::vector<std::string> menuLabels;
    int MENUITEM_TOGGLE_VISIBILITY;
    int MENUITEM_HISTORY_CLEAR;
    int MENUITEM_PRINT_ALL_RETURNED_VALUES;
    int MENUITEM_WARN_ABOUT_MULTIPLE_RETURNED_VALUES;
    int MENUITEM_STRING_ESCAPE_SPECIALS;
    int MENUITEM_MAP_SORT_KEYS_BY_NAME;
    int MENUITEM_MAP_SORT_KEYS_BY_TYPE;
    int MENUITEM_MAP_SHADOW_LONG_STRINGS;
    int MENUITEM_MAP_SHADOW_BUFFER_STRINGS;
    int MENUITEM_MAP_SHADOW_SPECIAL_STRINGS;
    int MENUITEM_HISTORY_SKIP_REPEATED;
    int MENUITEM_HISTORY_REMOVE_DUPS;
    int MENUITEM_SHOW_MATCHING_HISTORY;
    int MENUITEM_DYNAMIC_COMPLETION;
    int MENUITEM_AUTO_ACCEPT_COMMON_COMPLETION_PREFIX;
    int MENUITEM_RESIZE_STATUSBAR_WHEN_FOCUSED;
    static const int itemEnabled = 1, itemChecked = 2;
};

SIM_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
#include "stubsPlusPlus.cpp"
