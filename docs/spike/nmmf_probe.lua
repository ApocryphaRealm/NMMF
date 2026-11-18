-- NMMF spike, step 1: read the live Options screen - its tabs, the navigator's buttons, a tab's content
local function nm(o) local ok, s = pcall(function() return o:GetFullName() end); return ok and s or "?" end
local menus = FindAllOf("WBP_OptionsMenu_C") or {}
L("PROBE options menus: " .. #menus)
for _, m in ipairs(menus) do
    if m:IsValid() then
        L("PROBE menu " .. nm(m) .. " visible=" .. tostring(m:IsVisible()))
        local ok, err = pcall(function()
            local sw = m.TabSwitcher
            L("PROBE  TabSwitcher " .. nm(sw) .. " children " .. sw:GetChildrenCount() .. " active " .. sw:GetActiveWidgetIndex())
            for i = 0, sw:GetChildrenCount() - 1 do L("PROBE   tab " .. i .. " " .. nm(sw:GetChildAt(i))) end
        end); if not ok then L("PROBE  error: " .. tostring(err)) end
        local ok, err = pcall(function()
            local nav = m.NavigatorPanel
            L("PROBE  NavigatorPanel " .. nm(nav))
            local b = nav.Buttons
            for i = 1, #b do L("PROBE   nav button " .. i .. " " .. b[i]:get():ToString()) end
            local nb = nav.NavButtons
            L("PROBE   NavButtons " .. #nb)
        end); if not ok then L("PROBE  error: " .. tostring(err)) end
    end
end
local tabs = FindAllOf("WBP_OptionsTab_C") or {}
L("PROBE option tabs (base class instances): " .. #tabs)
for _, t in ipairs(tabs) do
    local ok, err = pcall(function()
        local content = t.Content
        local inner = content:GetChildAt(0)
        L("PROBE  tab " .. nm(t) .. " content child " .. nm(inner) .. " entries " .. (inner and inner:GetChildrenCount() or -1))
    end); if not ok then L("PROBE  error: " .. tostring(err)) end
end
