#pragma once
#include <cstdint>
namespace preview {
enum class RemoteReason {
    Unchecked,Pass,ReadFault,ClientTable,GameTable,GameInterface,NetworkTable,NetworkInterface,
    LocalPeer,LocalControl,LocalAddressVariant,LocalAddressPayload,Registration,HandlerTable,HandlerClient,Established,IdentifierType,GuidMissing,
    SessionVector,SessionMissing,SessionInactive,SessionOwner,SessionIdentity,SessionAmbiguous,SessionChanged,
    ModeTable,GameType,PlayerMissing,ModePlayer,PlayerMode,World,Target,HitMissing,HitType,HitPosition,HitFace,HandMissing,HandOccupied,HandTable,HandInvalid,HeldItemUnsupported,
    RegistryContract,RegistryMissing,RegistryExpired,RegistryRefused,RegistryFault,
    ChestTable,ChestContainerTable,ChestPairTable,ChestType,ChestPosition,ChestPartner,ChestPartnerY,ChestLead,ChestAdjacent,
    ShiftHeld,Sneaking,InputUnavailable
};
struct RemoteCheck {
    RemoteReason reason{RemoteReason::Unchecked};std::uint64_t observed{};
    friend bool operator==(RemoteCheck const&,RemoteCheck const&)=default;
};
constexpr char const* RemoteReasonName(RemoteReason r){
    switch(r){
    case RemoteReason::Unchecked:return "unchecked";case RemoteReason::Pass:return "pass";case RemoteReason::ReadFault:return "read-fault";
    case RemoteReason::ClientTable:return "client-table";case RemoteReason::GameTable:return "game-table";case RemoteReason::GameInterface:return "game-interface";
    case RemoteReason::NetworkTable:return "network-table";case RemoteReason::NetworkInterface:return "network-interface";
    case RemoteReason::LocalPeer:return "local-peer-owner";case RemoteReason::LocalControl:return "local-identifier-mismatch";
    case RemoteReason::LocalAddressVariant:return "local-address-variant-mismatch";case RemoteReason::LocalAddressPayload:return "local-address-payload-mismatch";
    case RemoteReason::SessionVector:return "local-session-vector";case RemoteReason::SessionMissing:return "local-session-missing";
    case RemoteReason::SessionInactive:return "local-session-inactive";case RemoteReason::SessionOwner:return "local-session-owner";
    case RemoteReason::SessionIdentity:return "local-session-identifier";case RemoteReason::SessionAmbiguous:return "local-session-ambiguous";
    case RemoteReason::SessionChanged:return "local-session-changed";
    case RemoteReason::Registration:return "registration-missing";case RemoteReason::HandlerTable:return "handler-table";
    case RemoteReason::HandlerClient:return "handler-client";case RemoteReason::Established:return "handler-not-established";
    case RemoteReason::IdentifierType:return "identifier-type";case RemoteReason::GuidMissing:return "guid-missing";
    case RemoteReason::ModeTable:return "game-mode-table";case RemoteReason::GameType:return "game-type";
    case RemoteReason::PlayerMissing:return "player-missing";
    case RemoteReason::ModePlayer:return "mode-player";case RemoteReason::PlayerMode:return "player-mode";
    case RemoteReason::World:return "player-world";case RemoteReason::Target:return "target-kind";
    case RemoteReason::HitMissing:return "hit-missing";case RemoteReason::HitType:return "hit-type";
    case RemoteReason::HitPosition:return "hit-position";case RemoteReason::HitFace:return "hit-face";
    case RemoteReason::HandMissing:return "hand-missing";case RemoteReason::HandOccupied:return "main-hand-not-empty";
    case RemoteReason::HandTable:return "hand-stack-table";case RemoteReason::HandInvalid:return "hand-invalid";
    case RemoteReason::HeldItemUnsupported:return "held-item-unsupported";
    case RemoteReason::RegistryContract:return "registry-contract";case RemoteReason::RegistryMissing:return "registry-missing";
    case RemoteReason::RegistryExpired:return "registry-expired";case RemoteReason::RegistryRefused:return "operation-refused";
    case RemoteReason::RegistryFault:return "registry-or-operation-fault";
    case RemoteReason::ChestTable:return "chest-table";case RemoteReason::ChestContainerTable:return "chest-container-table";
    case RemoteReason::ChestPairTable:return "chest-pair-table";case RemoteReason::ChestType:return "chest-type";
    case RemoteReason::ChestPosition:return "chest-position";case RemoteReason::ChestPartner:return "chest-partner";
    case RemoteReason::ChestPartnerY:return "chest-partner-y";case RemoteReason::ChestLead:return "chest-lead";
    case RemoteReason::ChestAdjacent:return "chest-adjacency";
    case RemoteReason::ShiftHeld:return "shift-held";case RemoteReason::Sneaking:return "sneaking-input";
    case RemoteReason::InputUnavailable:return "input-unavailable";
    }return "unknown";
}
enum class RemoteGate {NotRequested,Hud,BlockTarget,Route,Work,Region,PlayerBinding,CurrentModel,Chest,Policy,Registry,Live,UseHud,UseRegion,ModeRead,HandGetter,Use,UseChest,Submitted,ForeignOwner,UseInput};
constexpr char const* RemoteGateName(RemoteGate g){
    switch(g){
    case RemoteGate::NotRequested:return "not-requested";case RemoteGate::Hud:return "hud";case RemoteGate::BlockTarget:return "block-target";
    case RemoteGate::Route:return "route";case RemoteGate::Work:return "bridge-work";case RemoteGate::Region:return "region";
    case RemoteGate::PlayerBinding:return "player-binding";case RemoteGate::CurrentModel:return "existing-model";case RemoteGate::Chest:return "chest";
    case RemoteGate::Policy:return "policy";case RemoteGate::Registry:return "registry";case RemoteGate::Live:return "live-owner";
    case RemoteGate::UseHud:return "use-hud";case RemoteGate::UseRegion:return "use-region";case RemoteGate::ModeRead:return "mode-read";
    case RemoteGate::HandGetter:return "hand-getter";case RemoteGate::Use:return "use";case RemoteGate::UseChest:return "use-chest";
    case RemoteGate::Submitted:return "native-use-submitted";case RemoteGate::ForeignOwner:return "foreign-owner";
    case RemoteGate::UseInput:return "use-input";
    }return "unknown";
}
// Value-only state at the completed admission boundary. No pointer ownership.
struct RemoteGateSample {
    RemoteGate gate{RemoteGate::NotRequested};RemoteCheck check{};
    unsigned phase{},action{};
    std::uint64_t client{},world{},generation{};int x{},y{},z{};
    friend bool operator==(RemoteGateSample const&,RemoteGateSample const&)=default;
};
class RemoteGateBudget {
public:bool Admit(RemoteGateSample const& sample) noexcept {
    if(accepted_==128 || (accepted_ && sample==previous_))return false;
    previous_=sample;++accepted_;return true;
}
private:RemoteGateSample previous_{};unsigned accepted_{};
};
}
