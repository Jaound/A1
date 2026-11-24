#pragma once

#include <algorithm>
#include <array>
#include <vector>

namespace OVBase
{
constexpr int CellCm = 2500;

struct Cell
{
    int X = 0, Y = 0, Z = 0;
    bool operator==(const Cell& b) const { return X == b.X && Y == b.Y && Z == b.Z; }
    Cell operator+(const Cell& b) const { return {X+b.X,Y+b.Y,Z+b.Z}; }
};

enum class Kind { Core, Corridor, Junction, Storage };
enum class Error { None, Occupied, NoConnection, Funds, Limit, Busy, Outside };

struct Module
{
    Kind Type;
    Cell Origin;
    int Turns = 0;
    double Remaining = 0;
};

inline Cell Dimensions(Kind type)
{
    if (type == Kind::Core) return {5,5,5};
    if (type == Kind::Storage) return {2,2,2};
    return {1,1,1};
}

inline Cell Rotate(Cell p, int turns)
{
    for (int i = 0; i < turns; ++i) p = {-p.Y,p.X,p.Z};
    return p;
}

inline bool Contains(const Module& module, Cell cell)
{
    const auto d = Dimensions(module.Type);
    const auto p = Rotate(cell + Cell{-module.Origin.X,-module.Origin.Y,-module.Origin.Z}, (4-module.Turns)%4);
    return p.X >= 0 && p.X < d.X && p.Y >= 0 && p.Y < d.Y && p.Z >= 0 && p.Z < d.Z;
}

inline bool IsLink(Kind type) { return type == Kind::Corridor || type == Kind::Junction; }
inline int Cost(Kind type) { return IsLink(type) ? 40 : 300; }

struct Port { Cell At; Cell Direction; };

inline std::vector<Port> Ports(const Module& module)
{
    std::vector<Port> ports;
    const auto add = [&](Cell at, Cell direction) {
        ports.push_back({module.Origin + Rotate(at,module.Turns), Rotate(direction,module.Turns)});
    };
    if (module.Type == Kind::Core)
    {
        add({4,2,2},{1,0,0}); add({0,2,2},{-1,0,0});
        add({2,4,2},{0,1,0}); add({2,0,2},{0,-1,0});
        add({2,2,4},{0,0,1}); add({2,2,0},{0,0,-1});
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
            if (pa.At + pa.Direction == pb.At && pa.Direction + pb.Direction == Cell{}) return true;
    return false;
}

class BuildState
{
public:
    const std::vector<Module>& Modules() const { return Items; }
    int Funds() const { return Common; }

    Error Check(const Module& candidate) const
    {
        if (candidate.Type == Kind::Core || candidate.Turns < 0 || candidate.Turns >= 4) return Error::Outside;
        const Cell d = Dimensions(candidate.Type);
        for (int x=0; x<d.X; ++x)
            for (int y=0; y<d.Y; ++y)
                for (int z=0; z<d.Z; ++z)
                {
                    const auto p = candidate.Origin + Rotate({x,y,z}, candidate.Turns);
                    if (p.X < -24 || p.X > 24 || p.Y < -24 || p.Y > 24 || p.Z < -8 || p.Z > 8) return Error::Outside;
                    for (const auto& item : Items)
                        if (Contains(item,p)) return Error::Occupied;
                }

        bool attached = false;
        int links = 0, facilities = 0;
        for (const auto& item : Items)
        {
            attached |= Connected(candidate,item);
            if (IsLink(item.Type)) ++links;
            else if (item.Type != Kind::Core) ++facilities;
        }
        if (!attached) return Error::NoConnection;
        if (IsLink(candidate.Type) ? links >= 24 : facilities >= 8) return Error::Limit;
        if (!IsLink(candidate.Type) && std::any_of(Items.begin(), Items.end(), [](const Module& item) {
            return item.Type != Kind::Core && !IsLink(item.Type) && item.Remaining > 0;
        })) return Error::Busy;
        return Common < Cost(candidate.Type) ? Error::Funds : Error::None;
    }

    bool Start(Module candidate)
    {
        if (Check(candidate) != Error::None) return false;
        candidate.Remaining = IsLink(candidate.Type) ? 15 : 60;
        Items.push_back(candidate);
        Common -= Cost(candidate.Type);
        return true;
    }

    bool Cancel(std::size_t index)
    {
        if (index >= Items.size() || Items[index].Remaining <= 0) return false;
        const auto removed = Items[index];
        Items.erase(Items.begin()+index);
        const auto network = Network(true);
        if (std::find(network.begin(),network.end(),false) != network.end())
        {
            Items.insert(Items.begin()+index,removed);
            return false;
        }
        Common += Cost(removed.Type);
        return true;
    }

    std::vector<bool> Network(bool includePlanned) const
    {
        std::vector<bool> found(Items.size(), false);
        std::vector<std::size_t> queue{0};
        found[0] = true;
        for (std::size_t q=0; q<queue.size(); ++q)
            for (std::size_t j=1; j<Items.size(); ++j)
                if (!found[j] && (includePlanned || Items[j].Remaining <= 0) && Connected(Items[queue[q]],Items[j]))
                {
                    found[j] = true;
                    queue.push_back(j);
                }
        return found;
    }

    void Advance(double delta)
    {
        if (!(delta > 0)) return;
        // A reserved downstream module may be placed early. Construction waits for completed access.
        while (delta > 0)
        {
            const auto network = Network(false);
            std::vector<std::size_t> active;
            for (std::size_t i=1; i<Items.size(); ++i)
                if (Items[i].Remaining > 0)
                    for (std::size_t j=0; j<Items.size(); ++j)
                        if (network[j] && Connected(Items[i],Items[j]))
                        {
                            active.push_back(i);
                            break;
                        }
            if (active.empty()) return;
            double step = delta;
            for (const auto i : active) step = std::min(step,Items[i].Remaining);
            for (const auto i : active) Items[i].Remaining -= step;
            delta -= step;
        }
    }

private:
    std::vector<Module> Items{{Kind::Core,{-2,-2,-2}}};
    int Common = 1200;
};
}
