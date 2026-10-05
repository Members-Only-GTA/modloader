/*
 * Copyright (C) 2014  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 *
 *      Mod Loader Menu API
 *
 *      What the in-game menu (extras/gta3/menu.cpp) does, exported for a game's own frontend to draw it instead:
 *      the main page's options, the mods (sorted by name) with their Enabled and Priority, and the menu's
 *      enter / leave (the watcher paused while in it, the mods changed applied on leaving) and Rescan.
 *
 *      A name is a mod's name as Mod Loader keeps it (normalized). Everything is on the game's thread.
 */
#include <stdinc.hpp>
#include "loader.hpp"
using namespace modloader;

// The Loader's members the menu reaches (as TheMenu does)
struct MenuApi
{
    static Loader::FolderInformation& Mods()  { return loader.mods; }
    static bool& EnableLog()                  { return MenuApi::EnableLog(); }
    static bool& ImmediateFlush()             { return MenuApi::ImmediateFlush(); }
    static uint64_t& MaxBytesInLog()          { return MenuApi::MaxBytesInLog(); }
    static bool& AutoRefresh()                { return MenuApi::AutoRefresh(); }
};

namespace
{
    Loader::Journal changedMods;            // Mods that have changed from the current menu usage
    ref_list<Loader::ModInformation> mods;  // The mods listed, by name

    Loader::ModInformation* FindMod(const char* name)
    {
        if(name == nullptr) return nullptr;
        for(auto& mod : MenuApi::Mods().GetMods())
        {
            if(mod.get().GetName() == name)
                return &mod.get();
        }
        return nullptr;
    }

    enum Option
    {
        OptionEnableLog,
        OptionImmediateFlush,
        OptionMaxLogSize,       // bytes
        OptionAutoRefresh,
    };
}

extern "C"
{
    __declspec(dllexport) int ModLoaderMenu_Version()
    {
        return 1;
    }

    // Entering the menu: pause the FS watcher so we don't come up with any surprise
    __declspec(dllexport) void ModLoaderMenu_Enter()
    {
        loader.PauseWatcher();
        changedMods.clear();
    }

    // Getting out of the menu: apply the mods changed, and reset the watcher so it reflects the new bAutoRefresh state
    __declspec(dllexport) void ModLoaderMenu_Leave()
    {
        if(changedMods.size())
        {
            loader.UpdateFromJournal(changedMods);
            changedMods.clear();
        }
        loader.RestartWatcher();
    }

    // Rescan modifications
    __declspec(dllexport) void ModLoaderMenu_Rescan()
    {
        changedMods.clear();
        loader.ScanAndUpdate();
    }

    __declspec(dllexport) unsigned long long ModLoaderMenu_GetOption(int option)
    {
        switch(option)
        {
            case OptionEnableLog:      return MenuApi::EnableLog();
            case OptionImmediateFlush: return MenuApi::ImmediateFlush();
            case OptionMaxLogSize:     return MenuApi::MaxBytesInLog();
            case OptionAutoRefresh:    return MenuApi::AutoRefresh();
        }
        return 0;
    }

    __declspec(dllexport) void ModLoaderMenu_SetOption(int option, unsigned long long value)
    {
        static const uint64_t mb = 1048576;
        switch(option)
        {
            case OptionEnableLog:      MenuApi::EnableLog() = value != 0; break;
            case OptionImmediateFlush: MenuApi::ImmediateFlush() = value != 0; break;
            case OptionMaxLogSize:     MenuApi::MaxBytesInLog() = std::min(std::max<uint64_t>(value, mb), mb * 100); break;
            case OptionAutoRefresh:    MenuApi::AutoRefresh() = value != 0; break;
            default: return;
        }
        loader.SaveBasicConfig();
    }

    // Lists the mods by name; returns how many
    __declspec(dllexport) int ModLoaderMenu_ListMods()
    {
        mods = MenuApi::Mods().GetModsByName();
        return int(mods.size());
    }

    // The @index mod of the last list's name and folder (its path); false past the end
    __declspec(dllexport) bool ModLoaderMenu_ModAt(int index, const char** name, const char** path)
    {
        if(index < 0 || size_t(index) >= mods.size())
            return false;
        if(name) *name = mods[index].get().GetName().c_str();
        if(path) *path = mods[index].get().GetPath().c_str();
        return true;
    }

    __declspec(dllexport) bool ModLoaderMenu_GetModEnabled(const char* name)
    {
        auto* mod = FindMod(name);
        return mod && !mod->Parent().Profile().IsOnIgnoringList(mod->GetName());
    }

    __declspec(dllexport) void ModLoaderMenu_SetModEnabled(const char* name, bool enabled)
    {
        if(auto* mod = FindMod(name))
        {
            if(enabled) mod->Parent().Profile().UnignoreMod(mod->GetName());
            else mod->Parent().Profile().IgnoreMod(mod->GetName());
            changedMods.emplace(mod->GetName(), Loader::Status::Updated);
            loader.SaveFolderConfig();
        }
    }

    __declspec(dllexport) int ModLoaderMenu_GetModPriority(const char* name)
    {
        auto* mod = FindMod(name);
        return mod? mod->Parent().Profile().GetPriority(mod->GetName()) : 0;
    }

    __declspec(dllexport) void ModLoaderMenu_SetModPriority(const char* name, int priority)
    {
        if(auto* mod = FindMod(name))
        {
            priority = std::min(std::max(priority, 0), MenuApi::Mods().GetPriorityLimit());
            mod->Parent().Profile().SetPriority(mod->GetName(), priority);
            changedMods.emplace(mod->GetName(), Loader::Status::Updated);
            loader.SaveFolderConfig();
        }
    }

    __declspec(dllexport) int ModLoaderMenu_GetPriorityLimit()
    {
        return MenuApi::Mods().GetPriorityLimit();
    }
}
