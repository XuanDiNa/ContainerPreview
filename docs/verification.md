# Verification and binary provenance — 0.1.0

## English

### Fresh public-copy verification — 2026-10-23

The full public-copy verifier completed with exit 0. Both clean Debug/Release
builds succeeded: **48 C++ executions / 8742 checks / zero failures**, plus
**18 Python artifact tests**. No compiler warning/error was observed. All six
exact game images passed their profile tests and post-run hash checks.

The release ZIP is `ContainerPreview_0.1.0_MC26.45-26.51-26.52.zip`, 276631
bytes; SHA-256:
`BC2A899DEB38E457CCE8A58CEB34671E615FA672A1DB233EAF4D608251C7AC45`.
All eight unique flat member streams were checked against the frozen package;
all seven payloads match public inputs or the independently pinned retained DLL.
README SHA-256:
`2394C74398336AE6EE1DEC387EC8AA5A0C914D845CFCFC27BB459FCEA941A4F7`.

The fresh public Release DLL has SHA-256
`39178AB2DB5EF8AFC6183CF0A06702BBD47CB8051F983C90F5E0C51869D5984B`.
It differs from the accepted artifact and is not published. The release DLL
keeps the accepted hash below. Native files (36) and test files (16) match the
development inputs byte-for-byte. Runtime package checksums/defaults/metadata
and independent DLL provenance pass. Release identity is also recorded in
[release-artifacts-0.1.0.json](release-artifacts-0.1.0.json).

### Manual acceptance

The retained test31 DLL completed the focused manual round separately on
Minecraft 1.26.45.01, 1.26.51.01 and 1.26.52.03. Installed artifact hashes were
checked, and attributable startup/Ready, snapshot/render and background-session
observations corroborate the reports. Diagnostic counters are not independent
proof of every container, mode or request lifecycle.

| Exact game version | Focused manual result | Installed DLL |
|---|---|---|
| 1.26.45.01 | User accepted | Same retained test31 hash |
| 1.26.51.01 | User accepted | Same retained test31 hash |
| 1.26.52.03 | User accepted | Same retained test31 hash |

This does not certify all servers, custom containers, plugin combinations or
extended sessions. Additional single-player adapters do not all have individual
manual coverage. Experimental Hive owner-UID correlation limits remain; see
[compatibility.md](compatibility.md).

### Accepted release binary

- Package version: **0.1.0**.
- Binary startup identifier: **0.1.0-test31**, intentionally unchanged.
- DLL size: **544256 bytes**.
- DLL SHA-256:
  `92F2C95A28BB317FA6B2F6E2B02119CF300760E20769C1CCBCF4934F250324A8`.
- AMD64; exactly `ContainerPreviewInit` / `ContainerPreviewStatus` exports.
- Import libraries: bcrypt.dll, kernel32.dll and user32.dll.

Native source and the accepted DLL were not edited to rename the release.
Public source files and tests are copied byte-for-byte from the validated
development version. Clean rebuilds have separate provenance and are offline
verification outputs, not replacement gameplay-tested release artifacts.

### Artifact policy

The plugin ZIP contains exactly eight flat files: `ContainerPreview.dll`,
`config.json`, `config.schema.json`, `mod.json`, `manifest.json`, `README.md`,
`THIRD_PARTY_NOTICES.txt` and `SHA256SUMS.txt`. The internal checksum file covers
the seven payloads. External release assets add an archive `.sha256` and a
`SHA256SUMS.txt` covering the ZIP plus those seven payloads.

Both folder and ZIP verification check the exact metadata/game-version list,
config defaults/schema, PE architecture/exports/imports, complete checksums and
an independent accepted-DLL hash pin. Replacing the DLL and recalculating the
package checksums cannot satisfy that separate pin.

The repository is a clean allowlisted copy: source, tests, portable build and
verification tools, JSON dependency header and notices, settings and curated
documentation. No game binaries/assets, private analysis/IDA/decompilation,
runtime logs, credentials or machine-specific paths are distributed. The only
Windows user-path literal in the tests is a deliberately fictional negative
privacy fixture, not a real machine path. No installation or game automation
is part of publication.

## 繁體中文

### 公開副本離線驗證

公開副本完整流程已退出 0：Debug／Release 共 48 次 C++ 執行、8742 項檢查
零失敗，另有 18 項 Python 測試通過，未觀察到編譯警告／錯誤。六個精確
遊戲映像及執行後雜湊皆通過。ZIP、README、已驗收 DLL 及未發佈重編 DLL
的獨立雜湊見上方與[資產紀錄](release-artifacts-0.1.0.json)。36 個原生檔案
與 16 個測試檔逐檔一致；新 README 不改變插件行為。

### 人工驗收與 DLL 來源

保留的 test31 DLL 已在 1.26.45.01、1.26.51.01、1.26.52.03 分別完成本輪
人工驗收；已核對安裝雜湊及可歸屬的 Ready、快照／繪製、背景流程紀錄。
診斷計數不代表每個容器、模式或請求流程皆已獨立證實，也不保證所有
伺服器、插件組合或長時間情境。額外單人容器仍未全部逐項人工測試，Hive
擁有者 UID 限制也仍保留，詳見[相容性說明](compatibility.md)。

正式套件資訊為 0.1.0，原 DLL 啟動識別仍是 0.1.0-test31，大小與 SHA-256
見上方。沒有為改版本字串修改原生程式碼或 DLL。公開原始碼與測試逐檔
對應驗收時開發版本；乾淨重編僅作離線驗證，不取代已驗收的發佈 DLL。

### 封裝原則

插件 ZIP 僅八個平鋪檔案，內附七項內容校驗碼；Release 另提供 ZIP 校驗
檔，以及包含 ZIP 與七項內容的總校驗檔。驗證同時檢查版本、設定、PE、
匯出／依賴及獨立的 DLL 來源雜湊，不能重算套件校驗碼就替換已驗收 DLL。

公開儲存庫只包含白名單原始碼、測試、可攜工具、JSON 標頭與授權、設定
及整理後文件，不含遊戲檔、私人分析／IDA／反編譯資料、日誌、憑證或
實際電腦路徑。測試中的使用者路徑是故意虛構的隱私拒絕案例。發佈不涉及
安裝插件或自動操作遊戲。
