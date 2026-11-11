# Compatibility and limitations

[English](#english) · [繁體中文](#繁體中文)

## English

### Supported scope

- Windows x64 / LeviLauncher native DLL plugin.
- Exact Minecraft 1.26.45.01, 1.26.51.01 and 1.26.52.03 builds only.
- Ordinary and double chests, and the current player's own 27-slot Ender Chest.
- Permitted native background opening in Survival, Creative and Adventure.
- Additional single-player read-only adapters: barrels, shulker boxes, furnace
  variants, brewing stands, hoppers, dispensers/droppers, crafters, chiseled
  bookshelves and verified container entities. Not all have individual manual
  gameplay coverage.
- Experimental server entity path only for `hivesky:death_crate`.

Players, other players' inventories, arbitrary custom menus/entities,
item-tooltip previews, Spectator and unknown modes are excluded. Normal server
access and server rules still apply. An entity using a chest-like screen is not
automatically supported.

### Native safety and request lifecycle

Game-image, text and native-entry guards are separate per exact version.
Unsupported images or incomplete checks stay inactive; entries are not found
by runtime scanning or guessed version deltas. There are 27 hooks per profile;
45 has 87 disk / 86 loaded guard records, and 51/52 have 152 / 151.

The test5 prerelease adds the separately reviewed current 26.51 launcher image
while retaining its earlier exact hash. All original sections match the original
26.51 image byte-for-byte; [the review](launcher-26.51-review.json) records the
additional loader import section. This is an exact-image update, not support
for arbitrary launcher patches. New-DLL gameplay acceptance is still pending.

Server previews wait for matching native container acceptance and complete
applied contents. Only one operation can be pending. Timeouts, late-reply
fences, owner checks and manual-interaction priority remain in effect. For the
experimental Hive path, the returned owner UID is **not a unique request
token**: that UID alone cannot independently exclude unrelated or late replies
in the same context. This limitation has not been resolved by manual success.

Sneaking pauses new requests without changing the player's state. Standing can
resume the first unsubmitted request. If manual interaction takes over, release
and repress the preview key. Supported held items do not require clearing or
swapping the real hand; unverified special held items remain refused for block
containers.

### Background opening and No data

Background opening uses normal native interaction and access rules. It can
play sounds, animate lids, trigger opening events, and generate and save loot
in a previously unopened loot container. Preview is not a promise of a
side-effect-free peek. Not every No data result authorizes a request: unknown
targets, refused interactions and unsupported modes fail closed.

The plugin does not transfer items, write inventory memory, synthesize input,
construct packets, modify game executables or bypass server permissions.
Conflicting hooks can leave it inactive rather than replacing another plugin's
hook. No universal-server, zero-latency or unlimited-session guarantee is made.

### Diagnostics

`ContainerPreview.log` stays local and is not automatically uploaded. It can
include coordinates, numeric container/nonplayer actor IDs, native lifecycle
and rendering metadata. It does not intentionally log chat, server addresses,
credentials or item names/NBT. Review excerpts before sharing them.

## 繁體中文

### 支援範圍

- Windows x64／LeviLauncher 原生 DLL 插件。
- 僅限精確 Minecraft 1.26.45.01、1.26.51.01、1.26.52.03。
- 普通箱子、大箱子，以及目前玩家自己 27 格的終界箱。
- 生存、創造、冒險模式中正常允許的原生背景開啟。
- 額外單人唯讀適配：木桶、界伏盒、各種熔爐、釀造台、漏斗、發射器／
  投擲器、合成器、雕紋書櫃及核實的容器實體，尚未全部逐項人工實測。
- 伺服器實體路徑僅實驗性支援 `hivesky:death_crate`。

不支援玩家、其他玩家背包、任意自定義選單／實體、物品提示預覽、旁觀或
未知模式。仍須符合正常存取權限與伺服器規則；使用箱子外觀介面的實體
不代表自動相容。

### 原生保護與請求流程

各精確版本分別檢查遊戲映像、程式碼區段及原生入口。不支援或驗證不完整
就保持停用，不使用執行期掃描或推測版本位移。各版本有 27 個 hook；45
有 87 個磁碟／86 個載入入口保護紀錄，51／52 則為 152／151。

test5 預發佈版加入獨立核實的目前 26.51 啟動器映像，保留原先的精確雜湊。
原有區段均與 26.51 原版逐位元組相同，[核對紀錄](launcher-26.51-review.json)
記錄新增的載入器匯入區段。這是精確映像更新，不支援任意啟動器修改；新 DLL
仍待遊戲驗收。

伺服器預覽須等原生容器接受相符，且完整內容已套用；同時只允許一個
待處理操作。保留逾時、延遲回覆、擁有者及手動優先等檢查。Hive 實驗性
路徑回傳的擁有者 UID **不是唯一請求關聯碼**，不能只靠它排除相同環境內
的無關或延遲回覆。人工成功測試不代表已解決此限制。

蹲下會暫停新的請求，不改動玩家狀態；站起來可恢復尚未送出的第一次
請求。手動操作接管後需放開再按預覽鍵。支援的持物不必清空或切換實際
主手；方塊容器仍拒絕未核實的特殊持物。

### 背景開啟與 No data

背景開啟沿用正常原生互動與存取規則，可能播放聲音、顯示箱蓋動畫、觸發
開箱事件，以及生成並保存未開過的戰利品，並非保證無副作用的窺視。
不是所有 No data 都能送出請求；未知目標、拒絕互動或不支援模式均保守停用。

插件不搬物品、不改背包記憶體、不模擬輸入、不自製封包、不修改遊戲 EXE，
也不繞過權限。遇到衝突 hook 可能保持停用，不會替換其他插件的 hook。
不保證所有伺服器相容、零延遲或無限長時間運作。

### 診斷

`ContainerPreview.log` 只保存在本機，不會自動上傳。可能包含座標、數字
容器／非玩家實體 ID、原生流程與繪製資訊；不刻意記錄聊天、伺服器地址、
憑證或物品名稱／NBT。分享前請先檢查片段。
