#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <vector>

namespace OVBase
{
constexpr int CellCm = 2500;
struct Cell
{
    int X = 0, Y = 0, Z = 0;
    bool operator==(const Cell& b) const { return X == b.X && Y == b.Y && Z == b.Z; }
    Cell operator+(const Cell& b) const { return {X + b.X, Y + b.Y, Z + b.Z}; }
    Cell operator-(const Cell& b) const { return {X - b.X, Y - b.Y, Z - b.Z}; }
};

enum class Kind
{
    Core, Corridor, Junction, Storage, Yard, Dock, YardMedium, YardLarge, YardSuperheavy,
    DockMedium, DockLarge, DockSuperheavy, Logistics, Research, Academy, Crew, Defense, Radar, Shield, Count
};
enum class Technology { None, EngineeringII, EngineeringIII, EngineeringIV, HullMedium, HullLarge, HullSuperheavy, StorageII, StorageIII, SupportHull, Aviation, Missile, Strategic, CombinedAir, Count };
enum class Error { None, Occupied, Direction, NoConnection, Funds, Limit, Busy, Outside, Seam, Clearance, Ship, Tier, Research, Capacity, Dependency, Upgrade };
struct Resources
{
    int Common = 0, Advanced = 0, Rare = 0;
    Resources operator+(Resources b) const { return {Common + b.Common, Advanced + b.Advanced, Rare + b.Rare}; }
    Resources operator-(Resources b) const { return {Common - b.Common, Advanced - b.Advanced, Rare - b.Rare}; }
    Resources Refund() const { return {Common * 3 / 4, Advanced * 3 / 4, Rare * 3 / 4}; }
    Resources Positive() const { return {std::max(0, Common), std::max(0, Advanced), std::max(0, Rare)}; }
    bool Covers(Resources b) const { return Common >= b.Common && Advanced >= b.Advanced && Rare >= b.Rare; }
    bool operator==(Resources b) const { return Common == b.Common && Advanced == b.Advanced && Rare == b.Rare; }
};

struct Definition
{
    const char* Key;
    const wchar_t* Name;
    Cell Size;
    Resources Price;
    int Seconds;
    int CoreTier;
    Technology Required = Technology::None;
    int FlightCells = 0;
    bool YardBay = false;
};

inline constexpr Definition Catalog[] = {
    {"Core", L"중앙 코어", {5,5,5}, {}, 0, 1},
    {"Corridor", L"일자 통로", {1,1,1}, {40,0,0}, 15, 1},
    {"Junction", L"분기 허브", {1,1,1}, {40,0,0}, 15, 1},
    {"Storage", L"자원 창고", {2,2,2}, {300,0,0}, 60, 1},
    {"Yard", L"소형 조선소", {4,3,3}, {650,0,0}, 120, 1, Technology::None, 2, true},
    {"Dock", L"표준 접안", {4,3,2}, {350,0,0}, 75, 1, Technology::None, 2},
    {"YardMedium", L"중형 조선소", {6,4,4}, {1800,300,0}, 240, 2, Technology::HullMedium, 3, true},
    {"YardLarge", L"대형 조선소", {9,6,5}, {6000,1200,80}, 420, 3, Technology::HullLarge, 4, true},
    {"YardSuperheavy", L"초대형 조선소", {12,8,7}, {18000,4000,600}, 720, 4, Technology::HullSuperheavy, 5, true},
    {"DockMedium", L"중형 접안", {5,4,3}, {900,150,0}, 150, 2, Technology::None, 3},
    {"DockLarge", L"대형 접안", {7,5,4}, {2600,500,40}, 240, 3, Technology::None, 4},
    {"DockSuperheavy", L"초대형 접안", {10,7,6}, {8000,1800,200}, 420, 4, Technology::None, 5},
    {"Logistics", L"물류 터미널", {3,2,2}, {600,100,0}, 120, 2, Technology::None, 1},
    {"Research", L"전략 연구소", {2,2,2}, {450,0,0}, 90, 1},
    {"Academy", L"영웅 아카데미", {2,2,2}, {700,100,0}, 150, 2},
    {"Crew", L"인원 지원소", {2,2,2}, {350,0,0}, 75, 1},
    {"Defense", L"기지 방어 모듈", {2,2,2}, {400,0,0}, 90, 1},
    {"Radar", L"기지 레이더", {2,2,3}, {300,50,0}, 60, 2},
    {"Shield", L"기지 방호 모듈", {3,3,3}, {1000,200,0}, 180, 2}
};
static_assert(std::size(Catalog) == static_cast<int>(Kind::Count));

struct CoreRule { int Facilities, Links, Concurrent; Resources Price; int Seconds; Technology Required; };
inline constexpr CoreRule Cores[] = {
    {8,24,1,{},0,Technology::None},
    {16,48,2,{2000,150,0},300,Technology::EngineeringII},
    {28,84,3,{9000,1500,0},600,Technology::EngineeringIII},
    {44,132,4,{28000,6000,800},960,Technology::EngineeringIV}
};
struct StorageRule { int CoreTier; Resources Capacity, Price; int Seconds; Technology Required; };
inline constexpr StorageRule Warehouses[] = {
    {1,{6000,1500,300},Catalog[static_cast<int>(Kind::Storage)].Price,Catalog[static_cast<int>(Kind::Storage)].Seconds,Technology::None},
    {2,{12000,2800,500},{500,80,0},90,Technology::StorageII},
    {3,{22000,4500,750},{1200,250,20},120,Technology::StorageIII}
};
struct TechnologyRule { const wchar_t* Name; int CoreTier; Technology Prior; };
inline constexpr TechnologyRule Technologies[] = {
    {L"없음",1,Technology::None}, {L"기지공학 II",1,Technology::None},
    {L"기지공학 III",2,Technology::EngineeringII}, {L"기지공학 IV",3,Technology::EngineeringIII},
    {L"중형 함체",2,Technology::EngineeringII}, {L"대형 함체",3,Technology::HullMedium},
    {L"초대형 함체",4,Technology::HullLarge}, {L"창고 증설 II",2,Technology::None},
    {L"창고 증설 III",3,Technology::StorageII},
    {L"중형 지원함",2,Technology::HullMedium},{L"항공 운용",3,Technology::HullLarge},
    {L"일반 미사일",2,Technology::EngineeringII},{L"전략 미사일 함체",3,Technology::HullLarge},
    {L"복합 항공함체",4,Technology::HullSuperheavy}
};
inline constexpr Resources CoreCapacity{6000,1000,100};

struct Module
{
    Kind Type;
    Cell Origin;
    int Turns = 0;
    double Remaining = 0;
    int Id = 0;
    int Level = 1;
    int UpgradeLevel = 0;
    Resources Paid;
    Resources Reserved;
};

inline bool Known(Kind type) { return static_cast<int>(type) >= 0 && type < Kind::Count; }
inline const Definition& Def(Kind type) { return Catalog[static_cast<int>(type)]; }
inline Cell Dimensions(Kind type) { return Def(type).Size; }
inline int Cost(Kind type) { return Def(type).Price.Common; }
inline double Duration(Kind type) { return Def(type).Seconds; }
inline bool IsLink(Kind type) { return type == Kind::Corridor || type == Kind::Junction; }
inline bool HasBay(Kind type) { return Def(type).FlightCells > 0; }
inline bool Completed(const Module& module) { return module.Remaining <= 0 || module.UpgradeLevel > 0; }
inline bool InBounds(Cell p) { return p.X>=-24 && p.X<=24 && p.Y>=-24 && p.Y<=24 && p.Z>=-8 && p.Z<=8; }
inline Cell Rotate(Cell p, int turns)
{
    for (int i = 0; i < turns; ++i) p = {-p.Y, p.X, p.Z};
    return p;
}
inline bool Contains(const Module& module, Cell cell)
{
    const auto d = Dimensions(module.Type);
    const auto p = Rotate(cell - module.Origin, (4 - module.Turns) % 4);
    return p.X >= 0 && p.X < d.X && p.Y >= 0 && p.Y < d.Y && p.Z >= 0 && p.Z < d.Z;
}

inline Cell BayForward(Kind type) { return Def(type).YardBay ? Cell{-1,0,0} : Cell{0,1,0}; }
struct BayApproach { int AcrossCm, HeightCm; };
inline BayApproach Approach(Kind type)
{
    // Centerlines cleared by bidirectional corvette sweeps of each bay mesh.
    switch (type)
    {
    case Kind::YardMedium: return {3700,1900};
    case Kind::YardLarge: return {6200,2200};
    case Kind::YardSuperheavy: return {10400,3700};
    case Kind::DockMedium: return {5000,900};
    case Kind::DockLarge: return {7500,1100};
    case Kind::DockSuperheavy: return {11300,3400};
    default: return {CellCm,0};
    }
}
inline Cell BayMouth(Kind type)
{
    const auto path=Approach(type);
    const int across=(path.AcrossCm+CellCm/2)/CellCm, height=(path.HeightCm+CellCm/2)/CellCm;
    return Def(type).YardBay ? Cell{0,across,height} : Cell{across,Dimensions(type).Y-1,height};
}
inline bool InFlightLane(const Module& bay, Cell world)
{
    if (!HasBay(bay.Type)) return false;
    const Cell p = Rotate(world - bay.Origin, (4-bay.Turns)%4);
    const auto d = Dimensions(bay.Type);
    const bool yard = Def(bay.Type).YardBay;
    const int ahead = yard ? -p.X : p.Y-(d.Y-1);
    if (ahead <= 0 || p.Z < 0 || p.Z >= d.Z) return false;
    const int across = yard ? p.Y : p.X;
    if (ahead <= Def(bay.Type).FlightCells) return across >= 0 && across < (yard ? d.Y : d.X);
    // Keep every grid cell touched by the ship clear beyond the full-width apron.
    const auto path=Approach(bay.Type);
    constexpr int CorvetteHalfWidthCm=251, CorvetteHalfHeightCm=180;
    return std::abs(across*CellCm-path.AcrossCm)<=CellCm/2+CorvetteHalfWidthCm &&
           std::abs(p.Z*CellCm-path.HeightCm)<=CellCm/2+CorvetteHalfHeightCm;
}

struct Port { Cell At; Cell Direction; };
inline std::vector<Port> Ports(const Module& module)
{
    std::vector<Port> ports;
    const auto add = [&](Cell at, Cell direction) {
        ports.push_back({module.Origin + Rotate(at,module.Turns), Rotate(direction,module.Turns)});
    };
    const auto size = Dimensions(module.Type);
    if (module.Type == Kind::Core)
    {
        add({4,2,2},{1,0,0}); add({0,2,2},{-1,0,0});
        add({2,4,2},{0,1,0}); add({2,0,2},{0,-1,0});
    }
    else if (Def(module.Type).YardBay)
    {
        add({size.X-1,size.Y-1,0},{0,1,0}); add({size.X-1,0,0},{0,-1,0});
    }
    else if (HasBay(module.Type))
    {
        add({0,1,0},{-1,0,0}); add({size.X-1,1,0},{1,0,0});
    }
    else
    {
        add({},{-1,0,0});
        if (IsLink(module.Type)) add({},{1,0,0});
        if (module.Type == Kind::Junction)
        {
            add({},{0,1,0}); add({},{0,-1,0}); add({},{0,0,1}); add({},{0,0,-1});
        }
    }
    return ports;
}

inline bool Connected(const Module& a, const Module& b)
{
    if (!IsLink(a.Type) && !IsLink(b.Type)) return false;
    for (const auto& pa : Ports(a))
        for (const auto& pb : Ports(b))
            if (pa.At + pa.Direction == pb.At && pa.Direction + pb.Direction == Cell{} &&
                (!pa.Direction.Z || a.Turns == b.Turns)) return true;
    return false;
}

struct UpgradeOrder { int Level = 0; Resources Price; int Seconds = 0; int CoreTier = 1; Technology Required = Technology::None; };
struct Removal { Error Result = Error::Outside; Resources Refund; Resources Overflow; std::vector<int> Disconnected; };
class BuildState
{
public:
    explicit BuildState(bool chargeResources = true) : ChargeResources(chargeResources) {}
    const std::vector<Module>& Modules() const { return Items; }
    int Funds() const { return Stock.Common; }
    bool Pay(Resources price)
    {
        if (!price.Covers({}) || !Stock.Covers(price)) return false;
        Stock=Stock-price;
        return true;
    }
    Resources Available() const { return Stock; }
    bool Charges() const { return ChargeResources; }
    unsigned ResearchMask() const { return Research; }
    int NextModuleId() const { return NextId; }
    int CoreTier() const { return Items[0].Level; }
    CoreRule Limits() const { return Cores[CoreTier()-1]; }
    bool HasTechnology(Technology tech) const { return tech == Technology::None || (Research & (1u << static_cast<int>(tech))); }
    void SetTestAccounting(bool value) { ChargeResources = value; }
    void FillTestStock() { Stock = Stock + (Capacity() - Stock).Positive(); }

    bool UnlockTestTechnology(Technology tech)
    {
        if (tech <= Technology::None || tech >= Technology::Count) return false;
        const auto& rule = Technologies[static_cast<int>(tech)];
        if (CoreTier() < rule.CoreTier || !HasTechnology(rule.Prior)) return false;
        if ((tech==Technology::Strategic && !HasTechnology(Technology::Missile)) ||
            (tech==Technology::CombinedAir && !HasTechnology(Technology::Aviation))) return false;
        Research |= 1u << static_cast<int>(tech);
        return true;
    }

    std::vector<bool> Network(bool includePlanned) const
    {
        auto& found=CachedNetwork[includePlanned?1:0];
        if (!found.empty()) return found;
        found.assign(Items.size(),false);
        std::vector<std::size_t> queue{0};
        found[0] = true;
        for (std::size_t q = 0; q < queue.size(); ++q)
            for (std::size_t j = 1; j < Items.size(); ++j)
                if (!found[j] && (includePlanned || Completed(Items[j])) && Connected(Items[queue[q]], Items[j]))
                {
                    found[j] = true;
                    queue.push_back(j);
                }
        return found;
    }

    Resources Capacity() const
    {
        auto result = CoreCapacity;
        const auto reachable = Network(false);
        for (std::size_t i = 1; i < Items.size(); ++i)
            if (reachable[i] && Items[i].Type == Kind::Storage)
                result = result + Warehouses[Items[i].Level-1].Capacity;
        return result;
    }
    Resources ReservedStock() const
    {
        Resources result;
        for (const auto& item : Items) result = result + item.Reserved;
        return result;
    }
    int ActiveFacilities() const
    {
        return static_cast<int>(std::count_if(Items.begin(), Items.end(), [](const Module& item) {
            return !IsLink(item.Type) && item.Remaining > 0;
        }));
    }

    Error CheckSpace(const Module& candidate, int ignore = -1) const
    {
        const Cell d = Dimensions(candidate.Type);
        for (int x=0; x<d.X; ++x)
            for (int y=0; y<d.Y; ++y)
                for (int z=0; z<d.Z; ++z)
                {
                    const auto p = candidate.Origin + Rotate({x,y,z}, candidate.Turns);
                    if (!InBounds(p)) return Error::Outside;
                    for (std::size_t i = 0; i < Items.size(); ++i)
                    {
                        if (static_cast<int>(i) == ignore) continue;
                        if (Contains(Items[i], p)) return Error::Occupied;
                        if (InFlightLane(Items[i], p)) return Error::Clearance;
                    }
                }
        if (HasBay(candidate.Type))
        {
            const Cell face = candidate.Origin + Rotate(BayMouth(candidate.Type), candidate.Turns);
            const Cell forward = Rotate(BayForward(candidate.Type),candidate.Turns);
            Cell end = face;
            for (int i=0; i<Def(candidate.Type).FlightCells; ++i) end = end + forward;
            if (!InBounds(end)) return Error::Clearance;
            for (std::size_t i = 0; i < Items.size(); ++i)
            {
                if (static_cast<int>(i) == ignore) continue;
                const auto& other = Items[i];
                const auto size = Dimensions(other.Type);
                for (int x=0; x<size.X; ++x) for (int y=0; y<size.Y; ++y) for (int z=0; z<size.Z; ++z)
                    if (InFlightLane(candidate, other.Origin + Rotate({x,y,z}, other.Turns))) return Error::Clearance;
            }
        }
        return Error::None;
    }

    Error Check(const Module& candidate) const
    {
        if (!Known(candidate.Type) || candidate.Type == Kind::Core || candidate.Turns < 0 || candidate.Turns >= 4 || !InBounds(candidate.Origin))
            return Error::Outside;
        const auto& definition = Def(candidate.Type);
        if (CoreTier() < definition.CoreTier) return Error::Tier;
        if (!HasTechnology(definition.Required)) return Error::Research;
        if (const auto space = CheckSpace(candidate); space != Error::None) return space;
        int links = 0, facilities = 0;
        bool attached = false;
        for (const auto& item : Items)
        {
            attached |= Connected(candidate,item);
            if (IsLink(item.Type)) ++links;
            else if (item.Type != Kind::Core) ++facilities;
        }
        if (!attached)
        {
            for (const auto& item : Items)
                for (const auto& port : Ports(item))
                    if (Contains(candidate,port.At+port.Direction)) return Error::Direction;
            return Error::NoConnection;
        }
        const auto limits = Limits();
        if (IsLink(candidate.Type) ? links >= limits.Links : facilities >= limits.Facilities) return Error::Limit;
        if (!IsLink(candidate.Type) && ActiveFacilities() >= limits.Concurrent) return Error::Busy;
        return CheckPrice(definition.Price);
    }

    bool Start(Module candidate)
    {
        if (Check(candidate) != Error::None) return false;
        candidate = {candidate.Type, candidate.Origin, candidate.Turns, Duration(candidate.Type), NextId++};
        candidate.Reserved = ChargeResources ? Def(candidate.Type).Price : Resources{};
        Stock = Stock - candidate.Reserved;
        Items.push_back(candidate);
        InvalidateNetwork();
        return true;
    }

    UpgradeOrder Upgrade(std::size_t index) const
    {
        if (index >= Items.size()) return {};
        const auto& item = Items[index];
        if (item.Type == Kind::Core && item.Level < 4)
        {
            const auto& rule = Cores[item.Level];
            return {item.Level+1,rule.Price,rule.Seconds,item.Level,rule.Required};
        }
        if (item.Type==Kind::Research && item.Level<4)
            return {item.Level+1,{400*item.Level,80*item.Level,0},60*item.Level,item.Level+1,Technology::None};
        if (item.Type == Kind::Storage && item.Level < 3)
        {
            const auto& rule = Warehouses[item.Level];
            return {item.Level+1,rule.Price,rule.Seconds,rule.CoreTier,rule.Required};
        }
        return {};
    }

    Error CheckUpgrade(std::size_t index) const
    {
        const auto order = Upgrade(index);
        if (!order.Level || Items[index].Remaining > 0) return Error::Upgrade;
        if (!Network(false)[index]) return Error::NoConnection;
        if (CoreTier() < order.CoreTier) return Error::Tier;
        if (!HasTechnology(order.Required)) return Error::Research;
        if (ActiveFacilities() >= Limits().Concurrent) return Error::Busy;
        if (!Capacity().Covers(order.Price)) return Error::Capacity;
        return CheckPrice(order.Price);
    }

    bool StartUpgrade(std::size_t index)
    {
        if (CheckUpgrade(index) != Error::None) return false;
        const auto order = Upgrade(index);
        auto& item = Items[index];
        item.UpgradeLevel = order.Level;
        item.Remaining = order.Seconds;
        item.Reserved = ChargeResources ? order.Price : Resources{};
        Stock = Stock - item.Reserved;
        return true;
    }

    Removal InspectRemoval(std::size_t index, bool cancel) const
    {
        if (index >= Items.size()) return {};
        const auto& item = Items[index];
        if (cancel != (item.Remaining > 0) || (!cancel && item.Type == Kind::Core)) return {};
        if (item.UpgradeLevel > 0 && cancel) return {Error::None,item.Reserved};
        auto after = *this;
        after.Items.erase(after.Items.begin()+index);
        after.InvalidateNetwork();
        const auto wasPlanned = Network(true);
        const auto wasConnected = Network(false);
        const auto stillPlanned = after.Network(true);
        const auto stillConnected = after.Network(false);
        Removal preview{Error::None};
        for (std::size_t i=0; i<after.Items.size(); ++i)
        {
            const std::size_t before = i + (i >= index ? 1 : 0);
            if (after.Items[i].Remaining > 0 && wasPlanned[before] && !stillPlanned[i])
                preview.Result = Error::Dependency;
            if (wasConnected[before] && !stillConnected[i]) preview.Disconnected.push_back(after.Items[i].Id);
        }
        if (cancel) preview.Refund = item.Reserved;
        else
        {
            const auto desired = item.Paid.Refund();
            const auto room = (after.Capacity() - Stock).Positive();
            preview.Overflow = (desired - room).Positive();
            preview.Refund = desired - preview.Overflow;
        }
        return preview;
    }

    bool Remove(std::size_t index, bool cancel)
    {
        const auto preview = InspectRemoval(index,cancel);
        if (preview.Result != Error::None) return false;
        Stock = Stock + preview.Refund;
        if (cancel && Items[index].UpgradeLevel > 0)
        {
            auto& item = Items[index];
            item.UpgradeLevel = 0;
            item.Remaining = 0;
            item.Reserved = {};
        }
        else
        {
            Items.erase(Items.begin()+index);
            InvalidateNetwork();
        }
        return true;
    }
    bool Cancel(std::size_t index) { return Remove(index,true); }
    bool Demolish(std::size_t index) { return Remove(index,false); }

    void Advance(double delta, const std::vector<bool>& blocked = {})
    {
        if (!(delta > 0) || !std::isfinite(delta)) return;
        while (delta > 0)
        {
            const auto network = Network(false);
            std::vector<std::size_t> active;
            for (std::size_t i=0; i<Items.size(); ++i)
                if (Items[i].Remaining > 0 && (i>=blocked.size() || !blocked[i]))
                {
                    if (i==0 || (Items[i].UpgradeLevel > 0 && network[i])) active.push_back(i);
                    else if (!Items[i].UpgradeLevel)
                        for (std::size_t j=0; j<Items.size(); ++j)
                            if (network[j] && Connected(Items[i],Items[j]))
                            {
                                active.push_back(i);
                                break;
                            }
                }
            if (active.empty()) return;
            double step = delta;
            for (const auto i : active) step = std::min(step,Items[i].Remaining);
            bool completed=false;
            for (const auto i : active)
            {
                auto& item = Items[i];
                item.Remaining -= step;
                if (item.Remaining > 0) continue;
                completed=true;
                item.Paid = item.Paid + item.Reserved;
                item.Reserved = {};
                if (item.UpgradeLevel > 0) item.Level = item.UpgradeLevel;
                item.UpgradeLevel = 0;
            }
            if (completed) CachedNetwork[0].clear();
            delta -= step;
        }
    }

    bool Restore(std::vector<Module> modules, Resources stock, unsigned research, bool charge, int nextId)
    {
        if (modules.empty() || modules.size() > 177 || !stock.Covers({}) || !Resources{2000000,2000000,2000000}.Covers(stock) ||
            (research & ~((1u << static_cast<int>(Technology::Count))-2u)) || nextId < 2)
            return false;
        if (modules[0].Type != Kind::Core || !(modules[0].Origin == Cell{-2,-2,-2}) ||
            modules[0].Id != 1 || modules[0].Turns != 0) return false;
        std::vector<int> ids;
        for (std::size_t i=0; i<modules.size(); ++i)
        {
            const auto& item = modules[i];
            if (!Known(item.Type) || (i > 0 && item.Type == Kind::Core) || !InBounds(item.Origin) ||
                item.Turns < 0 || item.Turns >= 4 || item.Id < 1 || item.Id >= nextId ||
                std::find(ids.begin(), ids.end(), item.Id) != ids.end() ||
                !std::isfinite(item.Remaining) || item.Remaining < 0 || !item.Paid.Covers({}) || !item.Reserved.Covers({}) ||
                !Resources{50000,10000,2000}.Covers(item.Paid) || !Resources{50000,10000,2000}.Covers(item.Reserved))
                return false;
            const int maxLevel = (item.Type == Kind::Core || item.Type == Kind::Research) ? 4 : item.Type == Kind::Storage ? 3 : 1;
            if (item.Level < 1 || item.Level > maxLevel ||
                (item.UpgradeLevel && (item.UpgradeLevel != item.Level + 1 || item.UpgradeLevel > maxLevel || item.Remaining <= 0)) ||
                (!item.Remaining && !(item.Reserved == Resources{})) ||
                (i == 0 && item.Remaining > 0 && !item.UpgradeLevel)) return false;
            const double duration = item.UpgradeLevel ?
                (item.Type == Kind::Core ? Cores[item.Level].Seconds : item.Type==Kind::Research ? 60*item.Level : Warehouses[item.Level].Seconds) : Duration(item.Type);
            if (item.Remaining > duration) return false;
            ids.push_back(item.Id);
        }
        BuildState restored(charge);
        restored.Items = std::move(modules);
        restored.Stock = stock;
        restored.Research = research;
        restored.NextId = nextId;
        int links = 0, facilities = 0;
        for (std::size_t i=1; i<restored.Items.size(); ++i)
        {
            if (restored.CheckSpace(restored.Items[i],static_cast<int>(i)) != Error::None) return false;
            if (IsLink(restored.Items[i].Type)) ++links;
            else ++facilities;
        }
        const auto limits = restored.Limits();
        if (links > limits.Links || facilities > limits.Facilities || restored.ActiveFacilities() > limits.Concurrent) return false;
        for (const auto& item:restored.Items)
            if (Def(item.Type).CoreTier>restored.CoreTier() || !restored.HasTechnology(Def(item.Type).Required) ||
                (item.Type==Kind::Storage && Warehouses[item.Level-1].CoreTier>restored.CoreTier())) return false;
        *this = std::move(restored);
        return true;
    }

private:
    void InvalidateNetwork()
    {
        for (auto& network:CachedNetwork) network.clear();
    }
    Error CheckPrice(Resources price) const { return ChargeResources && !Stock.Covers(price) ? Error::Funds : Error::None; }
    std::vector<Module> Items{{Kind::Core,{-2,-2,-2},0,0,1}};
    mutable std::array<std::vector<bool>,2> CachedNetwork;
    bool ChargeResources;
    Resources Stock{1200,0,0};
    unsigned Research = 0;
    int NextId = 2;
};
}
