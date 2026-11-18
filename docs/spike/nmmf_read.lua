-- NMMF spike: read back the example switch and slider, and which tab is active
local live
for _, m in ipairs(FindAllOf("WBP_OptionsMenu_C") or {}) do
    if m:IsValid() and m:GetFullName():find("Transient") then live = m end
end
local ok, err = pcall(function()
    L("NMMFREAD active tab " .. live.TabSwitcher:GetActiveWidgetIndex())
    for _, t in ipairs(FindAllOf("WBP_OptionsEntry_Toggle_C") or {}) do
        if t:IsValid() and t.Name:ToString() == "NMMF example switch" then
            L("NMMFREAD switch CurrentIndex " .. tostring(t.CurrentIndex))
        end
    end
    for _, s in ipairs(FindAllOf("WBP_OptionsEntry_Slider_C") or {}) do
        if s:IsValid() and s.Name:ToString() == "NMMF example slider" then
            L("NMMFREAD slider CurrentValue " .. tostring(s.CurrentValue) .. " StepSize " .. tostring(s.StepSize))
        end
    end
end)
if not ok then L("NMMFREAD error: " .. tostring(err)) end
