-- NMMF spike, step 2: how the Gameplay tab holds its rows, and the navigator's labels
local function nm(o) local ok, s = pcall(function() return o:GetFullName():match("[^%.]+$") .. " (" .. o:GetClass():GetFName():ToString() .. ")" end); return ok and s or "?" end
local live
for _, m in ipairs(FindAllOf("WBP_OptionsMenu_C") or {}) do
    if m:IsValid() and m:GetFullName():find("Transient") then live = m end
end
if not live then L("PROBE2 no live options menu"); return end
local ok, err = pcall(function()
    local nav = live.NavigatorPanel
    local b = nav.Buttons
    local n = b:GetArrayNum()
    local labels = {}
    for i = 1, n do labels[#labels + 1] = b[i]:ToString() end
    L("PROBE2 nav Buttons (" .. n .. "): " .. table.concat(labels, ", "))
    L("PROBE2 nav NavButtons: " .. nav.NavButtons:GetArrayNum())
end); if not ok then L("PROBE2 nav error: " .. tostring(err)) end
local function walk(w, depth, maxd)
    if depth > maxd then return end
    local okc, cnt = pcall(function() return w:GetChildrenCount() end)
    L("PROBE2 " .. string.rep("  ", depth) .. nm(w) .. (okc and (" children " .. cnt) or ""))
    if okc then for i = 0, math.min(cnt, 8) - 1 do walk(w:GetChildAt(i), depth + 1, maxd) end end
end
local ok2, err2 = pcall(function()
    local tab = live.TabSwitcher:GetChildAt(3)
    L("PROBE2 gameplay tab " .. nm(tab))
    walk(tab.ScrollBox, 0, 2)
    local okn, slot = pcall(function() return tab.Content end)
    L("PROBE2 Content slot " .. (okn and nm(slot) or "none"))
    if okn then walk(slot, 0, 2) end
end); if not ok2 then L("PROBE2 tab error: " .. tostring(err2)) end
