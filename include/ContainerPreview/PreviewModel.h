#pragma once
#include "Config.h"
#include <cstdint>
#include <optional>
namespace preview {
enum class TargetKind { None, Block, Entity, Player };
struct Target {
    std::uint64_t world{}, unique{}, runtime{};
    int dimension{}, x{}, y{}, z{};
    TargetKind kind{};
    std::uint64_t entity{};
    std::uint64_t entityRuntime{};
    bool operator==(Target const&) const = default;
};
struct Eligibility { bool foreground{}, hud{}, held{}, local{}; };
struct Request { Target target; std::uint64_t generation{}, time{}; };
enum class Contents { Unknown, Ready, Absent };
struct RemoteLease {
    std::uintptr_t client{},player{},model{},control{};
    std::uint32_t owner{};
    unsigned id{};
    bool Valid() const {return client && player && model && control && owner && id>0 && id<=100;}
    bool operator==(RemoteLease const&) const = default;
};
struct Snapshot {
    Target target;
    std::uint64_t generation{}, time{};
    bool pairedServer{};
    Contents contents{};
    unsigned slots{};
    RemoteLease remote;
};
bool HasPreviewSource(Snapshot const&);
enum class Display { Hidden, Waiting, Grid };
struct Grid {
    float x{}, y{}, width{}, height{}, cell{};
    unsigned columns{}, rows{};
};
std::optional<Grid> LayoutGrid(unsigned slots, float width, float height);
class PreviewModel {
public:
    void Update(Config const&, Eligibility, Target const&, std::uint64_t now);
    std::optional<Request> Current(std::uint64_t now) const;
    Display View(Snapshot const&, std::uint64_t now) const;
private:
    std::optional<Request> request_;
    std::uint64_t generation_{};
};
}
