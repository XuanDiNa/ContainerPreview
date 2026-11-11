# Build and verification

## English

Use Windows x64, Visual Studio 2022 C++ build tools, xmake and Python 3.11
(`py -3.11`). xmake resolves MinHook 1.3.4; the nlohmann JSON header is vendored.
Third-party license text is retained in `THIRD_PARTY_NOTICES.txt`.

```powershell
xmake f -y -m release
xmake build -r -y
py -3.11 -m unittest discover -s tests -p test_artifacts.py
```

The current source is the **0.1.1-test4-leftalt prerelease**. To run the core
tests after a build, use `xmake run ContainerPreviewCoreTests`; the other
`ContainerPreview*Tests` targets are listed in `xmake.lua`. The binding SDK is
header-only and has no ModMenu DLL link dependency. For the publication test
scope and binary provenance, see [verification](verification.md).

The historical full verifier requires `$gameImages` as an array of
six legally obtained local paths: the original and LeviLauncher image for
each of the three exact supported versions. Images are read-only inputs; do
not add them to Git or distribute them. No local machine paths are embedded in
the verifier.

```powershell
./tools/verify_release.ps1 -GameImages $gameImages
```

That verifier requires **all six reviewed hashes**. If a local launcher image
has changed, do not add its hash or bypass the requirement. Run the profile
target against each reviewed image and use its `reject` argument for an
unreviewed fixture; record partial coverage explicitly. This publication's
five-accepted/one-rejected matrix is in the verification record.

The historical 0.1.0 full run executed 48 C++ programs (8742 checks) and
18 Python artifact tests. Current per-target counts differ. Profile tests inspect image bytes and data-only
fixtures without executing or patching game code. Offline checks cannot prove
rendering, networking or gameplay behavior of a newly built DLL.

To audit a downloaded release without rebuilding or needing game images:

```powershell
py -3.11 tools/verify_artifacts.py --zip ./ContainerPreview_0.1.0_MC26.45-26.51-26.52.zip --required-dll-sha256 92F2C95A28BB317FA6B2F6E2B02119CF300760E20769C1CCBCF4934F250324A8
```

Stable `-Package` requires `-RetainedDll` pointing to the exact accepted binary.
It refuses an unrelated or freshly rebuilt DLL even if it has valid exports.
The release metadata is 0.1.0; the accepted binary intentionally retains the
0.1.0-test31 startup line. A rebuild has its own provenance and must receive
fresh gameplay acceptance before being substituted into a future release.
Historical test31 artifact auditing requires `--expected-version 0.1.0-test31`.

The `-Package` path and eight-flat-member artifact verifier above apply to
**the v0.1.0 source tag only**, not the current update-only prerelease layout.
The new ZIP deliberately omits `config.json`; verify its external `.sha256`,
the checksums in `mods/ContainerPreview`, and the independently pinned DLL hash
in [keybindings.md](keybindings.md). Do not package a fresh rebuild as the frozen
candidate: a build is not proof of identical bytes or gameplay acceptance.

## 繁體中文

使用 Windows x64、Visual Studio 2022 C++ 建置工具、xmake 與 Python 3.11
（`py -3.11`）。xmake 取得 MinHook 1.3.4；專案附有 nlohmann JSON 標頭，
第三方授權保留於 `THIRD_PARTY_NOTICES.txt`。建置及測試命令見上方。

完整驗證需以 `$gameImages` 傳入六個合法取得的本機映像路徑：三個支援
版本各自的原版與 LeviLauncher 映像。這些是唯讀輸入，不能提交或散布。
驗證腳本沒有寫死私人電腦路徑。

目前原始碼為 0.1.1-test4-leftalt 預發佈版，可逐個執行 `xmake.lua` 的測試目標；
SDK 僅標頭，無需連結 ModMenu DLL。歷史完整腳本要求六個已審核雜湊，若本機
啟動器映像不同，不可直接加入白名單或跳過要求；請逐檔執行 ProfileTests，
未核實映像加 `reject`，並明確記錄未覆蓋的項目。

歷史正式版完整流程重編 Debug／Release，執行 48 次 C++ 測試（8742 項檢查）及 18 項
Python 封裝測試。版本測試只檢查映像位元組與資料模擬，不執行或修改遊戲
程式碼。離線通過不代表新 DLL 的繪製、網路或遊戲行為已人工通過。

下載套件可單獨用上方 `--zip` 命令核對，不必重編或提供遊戲映像。
正式封裝的 `-Package` 必須提供 `-RetainedDll`，且符合已驗收的固定雜湊；
即使新編 DLL 的匯出入口正確也不能取代它。套件版本為 0.1.0，原 DLL
啟動字串仍為 0.1.0-test31。未來若更換 DLL，需重新取得遊戲內驗收。

上述 `-Package` 與八檔平鋪套件驗證僅適用於 **v0.1.0 原始碼標籤**，不可直接
套用到目前升級包。新包刻意不含 `config.json`；請核對外部 ZIP 校驗檔、
`mods/ContainerPreview` 內的逐檔校驗，以及文件中的獨立 DLL 雜湊。重編檔不能
當作原凍結候選檔；離線建置不證明位元組相同或遊戲驗收通過。
