-- NMMF spike, step 3: a "Mods" tab in the live Options screen, built from the game's own widget classes
local function step(msg) L("NMMFADD " .. msg) end
local live
for _, m in ipairs(FindAllOf("WBP_OptionsMenu_C") or {}) do
    if m:IsValid() and m:GetFullName():find("Transient") then live = m end
end
if not live then step("no live options menu"); return end
local ok, err = pcall(function()
    local wbl = StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")
    local P = "/Game/Norse/UI/Widgets/Core/OptionsMenu/"
    local TabC = StaticFindObject(P .. "WBP_OptionsTab.WBP_OptionsTab_C")
    local ToggleC = StaticFindObject(P .. "WBP_OptionsEntry_Toggle.WBP_OptionsEntry_Toggle_C")
    local SliderC = StaticFindObject(P .. "WBP_OptionsEntry_Slider.WBP_OptionsEntry_Slider_C")
    local VBoxC = StaticFindObject("/Script/UMG.VerticalBox")
    step("classes tab " .. tostring(TabC:IsValid()) .. " toggle " .. tostring(ToggleC:IsValid()) .. " slider " .. tostring(SliderC:IsValid()) .. " vbox " .. tostring(VBoxC:IsValid()))
    local pc = FindFirstOf("PlayerController")
    local tab = wbl:Create(live, TabC, pc)
    step("tab created " .. tab:GetFullName():match("[^%.]+$"))
    local vbox = StaticConstructObject(VBoxC, tab.WidgetTree)
    tab.Content:SetContent(vbox)
    step("content set: " .. tab.Content:GetChildrenCount() .. " child")

    local t = wbl:Create(live, ToggleC, pc)
    t.Name = FText("NMMF example switch")
    t.Description = FText("A switch made by NMMF from the game's own on/off row.")
    vbox:AddChildToVerticalBox(t)
    step("toggle added")

    local s = wbl:Create(live, SliderC, pc)
    s.Name = FText("NMMF example slider")
    s.Description = FText("A slider made by NMMF: one nudge = one step.")
    s.MinValue = 0
    s.MaxValue = 10
    s.StepSize = 1
    s.CurrentValue = 5
    s.IncludeCheckbox = false
    vbox:AddChildToVerticalBox(s)
    step("slider added")

    local sw = live.TabSwitcher
    sw:AddChild(tab)
    step("switcher children " .. sw:GetChildrenCount())

    local nav = live.NavigatorPanel
    local b = nav.Buttons
    local n = b:GetArrayNum()
    b[n + 1] = FText("Mods")
    nav:SetNavButtons()
    step("nav buttons " .. nav.Buttons:GetArrayNum() .. " / NavButtons " .. nav.NavButtons:GetArrayNum())
end)
if not ok then step("error: " .. tostring(err)) end
