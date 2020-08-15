#include "sipp.h"

void SIPP::clear()
{
    open.clear();
    close.clear();
    collision_intervals.clear();
    landmarks.clear();
    constraints.clear();
    positive_constraints.clear();
    path.cost = -1;
}

double SIPP::dist(const Node& a, const Node& b)
{
    return std::sqrt(pow(a.i - b.i, 2) + pow(a.j - b.j, 2));
}

void SIPP::find_successors(Node curNode, const Map &map, std::list<Node> &succs, Heuristic &h_values, Node goal)
{
    Node newNode;
    std::vector<Node> valid_moves = map.get_valid_moves(curNode.id);
    for(auto move : valid_moves)
    {
        newNode.i = move.i;
        newNode.j = move.j;
        newNode.id = move.id;
        double cost = dist(curNode, newNode);
        newNode.g = curNode.g + cost;
        std::vector<std::pair<double, double>> intervals(0);
        auto colls_it = collision_intervals.find(newNode.id);
        if(colls_it != collision_intervals.end())
        {
            std::pair<double, double> interval = {0, CN_INFINITY};
            for(unsigned int i = 0; i < colls_it->second.size(); i++)
            {
                interval.second = colls_it->second[i].first;
                intervals.push_back(interval);
                interval.first = colls_it->second[i].second;
            }
            interval.second = CN_INFINITY;
            intervals.push_back(interval);
        }
        else
            intervals.push_back({newNode.g, CN_INFINITY});
        auto cons_it = constraints.find({curNode.id, newNode.id});
        for(auto interval: intervals)
        {
            if(interval.second < newNode.g)
                continue;
            if(interval.first > newNode.g)
                newNode.g = interval.first;
            if(cons_it != constraints.end())
                for(unsigned int i = 0; i < cons_it->second.size(); i++)
                    if(newNode.g - cost + CN_EPSILON > cons_it->second[i].t1 && newNode.g - cost < cons_it->second[i].t2)
                        newNode.g = cons_it->second[i].t2 + cost;
            newNode.interval = interval;
            if(newNode.g - cost > curNode.interval.second || newNode.g > newNode.interval.second)
                continue;
            if(goal.id == agent.goal_id) //perfect heuristic is known
                newNode.f = newNode.g + h_values.get_value(newNode.id, agent.id);
            else
            {
                double h = sqrt(pow(goal.i - newNode.i, 2) + pow(goal.j - newNode.j, 2));
                for(int i = 0; i < h_values.get_size(); i++) //differential heuristic with up to 10 pivots
                    h = std::max(h, fabs(h_values.get_value(newNode.id, i) - h_values.get_value(goal.id, i)));
                newNode.f = newNode.g + h;
            }
            succs.push_back(newNode);
        }
    }
}

Node SIPP::find_min()
{
    Node min = *open.begin();
    open.pop_front();
    return min;
}

void SIPP::add_open(Node newNode)
{
    std::list<Node>::iterator iter, pos;
    bool pos_found = false;
    if (open.empty())
    {
        open.push_back(newNode);
        return;
    }
    for(iter = open.begin(); iter != open.end(); ++iter)
    {
        if (!pos_found)
        {
            if(iter->f > newNode.f + CN_EPSILON) // if newNode.f has lower f-value
            {
                pos = iter;
                pos_found = true;
            }
            else if(fabs(iter->f - newNode.f) < CN_EPSILON && newNode.g + CN_EPSILON > iter->g) // if f-values are equal, compare g-values
            {
                pos = iter;
                pos_found = true;
            }
        }
        if (iter->id == newNode.id && fabs(iter->interval.second - newNode.interval.second) < CN_EPSILON)
        {
            if(newNode.f > iter->f - CN_EPSILON)
                return;
            if(pos == iter)
            {
                iter->f = newNode.f;
                iter->g = newNode.g;
                iter->interval = newNode.interval;
                iter->parent = newNode.parent;
                return;
            }
            open.erase(iter);
            break;
        }
    }
    if(pos_found)
        open.insert(pos, newNode);
    else
        open.push_back(newNode);
    return;
}

std::vector<Node> SIPP::reconstruct_path(Node curNode)
{
    path.nodes.clear();
    if(curNode.parent != nullptr)
    do
    {
        path.nodes.insert(path.nodes.begin(), curNode);
        curNode = *curNode.parent;
    }
    while(curNode.parent != nullptr);
    path.nodes.insert(path.nodes.begin(), curNode);
    for(unsigned int i = 0; i < path.nodes.size(); i++)
    {
        unsigned int j = i + 1;
        if(j == path.nodes.size())
            break;
        if(fabs(path.nodes[j].g - path.nodes[i].g - dist(path.nodes[j], path.nodes[i])) > CN_EPSILON)
        {
            Node add = path.nodes[i];
            add.g = path.nodes[j].g - dist(path.nodes[j], path.nodes[i]);
            path.nodes.emplace(path.nodes.begin() + j, add);
        }
    }
    return path.nodes;
}

void SIPP::add_collision_interval(int id, std::pair<double, double> interval)
{
    std::vector<std::pair<double, double>> intervals(0);
    if(collision_intervals.count(id) == 0)
        collision_intervals.insert({id, {interval}});
    else
        collision_intervals[id].push_back(interval);
    std::sort(collision_intervals[id].begin(), collision_intervals[id].end());
    for(int i = 0; i + 1 < collision_intervals[id].size(); i++)
        if(collision_intervals[id][i].second - CN_EPSILON < collision_intervals[id][i+1].first)
        {
            collision_intervals[id][i].second = collision_intervals[id][i+1].second;
            collision_intervals[id].erase(collision_intervals[id].begin() + i + 1);
            i--;
        }
}

void SIPP::add_move_constraint(Move move)
{
    std::vector<Move> m_cons(0);
    if(constraints.count({move.id1, move.id2}) == 0)
        constraints.insert({{move.id1, move.id2}, {move}});
    else
    {
        m_cons = constraints.at({move.id1, move.id2});
        bool inserted(false);
        for(unsigned int i = 0; i < m_cons.size(); i++)
        {
            if(inserted)
                break;
            if(m_cons[i].t1 > move.t1)
            {
                if(m_cons[i].t1 < move.t2 + CN_EPSILON)
                {
                    m_cons[i].t1 = move.t1;
                    if(move.t2 + CN_EPSILON > m_cons[i].t2)
                        m_cons[i].t2 = move.t2;
                    inserted = true;
                    if(i != 0)
                        if(m_cons[i-1].t2 + CN_EPSILON > move.t1 && m_cons[i-1].t2 < move.t2 + CN_EPSILON)
                        {
                            m_cons[i-1].t2 = move.t2;
                            if(m_cons[i-1].t2 + CN_EPSILON > m_cons[i].t1 && m_cons[i-1].t2 < m_cons[i].t2 + CN_EPSILON)
                            {
                                m_cons[i-1].t2 = m_cons[i].t2;
                                m_cons.erase(m_cons.begin() + i);
                            }
                            inserted = true;
                        }
                }
                else
                {
                    if(i != 0)
                        if(m_cons[i-1].t2 + CN_EPSILON > move.t1 && m_cons[i-1].t2 < move.t2 + CN_EPSILON)
                        {
                            m_cons[i-1].t2 = move.t2;
                            inserted = true;
                            break;
                        }
                    m_cons.insert(m_cons.begin() + i, move);
                    inserted = true;
                }
            }
        }
        if(m_cons.back().t2 + CN_EPSILON > move.t1 && m_cons.back().t2 < move.t2 + CN_EPSILON)
            m_cons.back().t2 = move.t2;
        else if(!inserted)
            m_cons.push_back(move);
        constraints.at({move.id1, move.id2}) = m_cons;
    }
}

void SIPP::make_constraints(std::list<Constraint> &cons)
{
    for(auto con : cons)
    {
        if(con.positive == false)
        {
            if(con.id1 == con.id2) // wait consatraint
                add_collision_interval(con.id1, std::make_pair(con.t1, con.t2));
            else
                add_move_constraint(Move(con));
        }
        else if(con.agent == agent.id)
        {
            bool inserted = false;
            for(int i = 0; i < landmarks.size(); i++)
                if(landmarks[i].t1 > con.t1)
                {
                    landmarks.insert(landmarks.begin() + i, Move(con.t1, con.t2, con.i1, con.j1, con.i2, con.j2, con.id1, con.id2));
                    inserted = true;
                    break;
                }
            if(!inserted)
                landmarks.push_back(Move(con.t1, con.t2, con.i1, con.j1, con.i2, con.j2, con.id1, con.id2));
        }
        else
        {
            positive_constraints.push_back(Move(con.t1, con.t2, con.i1, con.j1, con.i2, con.j2, con.id1, con.id2));
        }
    }
}

Path SIPP::add_part(Path result, Path part)
{
    part.nodes.erase(part.nodes.begin());
    for(auto n: part.nodes)
        result.nodes.push_back(n);
    return result;
}

std::vector<Path> SIPP::find_partial_path(std::vector<Node> starts, std::vector<Node> goals, const Map &map, Heuristic &h_values, double max_f)
{
    open.clear();
    close.clear();
    path.cost = -1;
    std::vector<Path> paths(goals.size());
    int pathFound(0);
    for(auto s:starts)
    {
        s.parent = nullptr;
        open.push_back(s);
    }
    Node curNode;
    while(!open.empty())
    {
        curNode = find_min();
        close.insert({curNode.id, curNode});
        if(curNode.id == goals[0].id)
        {
            for(int i = 0; i < goals.size(); i++)
                if(curNode.g - CN_EPSILON < goals[i].interval.second && goals[i].interval.first - CN_EPSILON < curNode.interval.second)
                {
                    paths[i].nodes = reconstruct_path(curNode);
                    if(paths[i].nodes.back().g < goals[i].interval.first)
                    {
                        curNode.g = goals[i].interval.first;
                        paths[i].nodes.push_back(curNode);
                    }
                    paths[i].cost = curNode.g;
                    paths[i].expanded = close.size();
                    pathFound++;
                }
            if(pathFound == goals.size())
                return paths;
        }
        std::list<Node> succs;
        succs.clear();
        find_successors(curNode, map, succs, h_values, Node(goals[0].id, 0, 0, goals[0].i, goals[0].j));
        std::list<Node>::iterator it = succs.begin();
        auto parent = &(close.find(curNode.id)->second);
        while(it != succs.end())
        {
            if(it->f > max_f)
            {
                it++;
                continue;
            }
            bool has = false;
            it->parent = parent;
            auto range = close.equal_range(it->id);
            for(auto i = range.first; i != range.second; i++)
                if(i->second.interval.first - CN_EPSILON < it->interval.first && i->second.interval.second + CN_EPSILON > it->interval.second)
                {
                    has = true;
                    break;
                }
            if(!has)
                add_open(*it);
            it++;
        }
    }
    return paths;
}

std::vector<Node> SIPP::get_endpoints(int node_id, double node_i, double node_j, double t1, double t2)
{
    std::vector<Node> nodes;
    nodes = {Node(node_id, 0, 0, node_i, node_j, nullptr, t1, t2)};
    if(collision_intervals[node_id].empty())
        return nodes;
    else
    {
        int i(0);
        for(int k = 0; k < collision_intervals[node_id].size(); k++)
            while(i < nodes.size())
            {
                Node n = nodes[i];
                auto c = collision_intervals[node_id][k];
                bool changed = false;
                if(c.first - CN_EPSILON < n.interval.first && c.second + CN_EPSILON > n.interval.second)
                {
                    nodes.erase(nodes.begin() + i);
                    changed = true;
                }
                else if(c.first - CN_EPSILON < n.interval.first && c.second > n.interval.first)
                {
                    nodes[i].interval.first = c.second;
                    changed = true;
                }
                else if(c.first - CN_EPSILON > n.interval.first && c.second + CN_EPSILON < n.interval.second)
                {
                    nodes[i].interval.second = c.first;
                    nodes.insert(nodes.begin() + i + 1, Node(node_id, 0, 0, node_i, node_j, nullptr, c.second, n.interval.second));
                    changed = true;
                }
                else if(c.first < n.interval.second && c.second + CN_EPSILON > n.interval.second)
                {
                    nodes[i].interval.second = c.first;
                    changed = true;
                }
                if(changed)
                {
                    i = -1;
                    k = 0;
                }
                i++;
            }
    }
    return nodes;
}

double SIPP::check_endpoint(Node start, Node goal)
{
    double cost = sqrt(pow(start.i - goal.i, 2) + pow(start.j - goal.j, 2));
    if(start.g + cost < goal.interval.first)
        start.g = goal.interval.first - cost;
    if(constraints.count({start.id, goal.id}) != 0)
    {
        auto it = constraints.find({start.id, goal.id});
        for(int i = 0; i < it->second.size(); i++)
            if(start.g + CN_EPSILON > it->second[i].t1 && start.g < it->second[i].t2)
                start.g = it->second[i].t2;
    }
    if(start.g > start.interval.second || start.g + cost > goal.interval.second)
        return CN_INFINITY;
    else
        return start.g + cost;
}

Path SIPP::find_path(Agent agent, const Map &map, std::list<Constraint> cons, Heuristic &h_values, int tree_size)
{
    this->clear();
    this->agent = agent;
    make_constraints(cons);

    std::vector<Node> starts, goals;
    std::vector<Path> parts, results, new_results;
    Path part, result;
    if(!landmarks.empty())
    {
        for(int i = 0; i <= landmarks.size(); i++)
        {
            if(i == 0)
            {
                starts = {get_endpoints(agent.start_id, agent.start_i, agent.start_j, 0, CN_INFINITY).at(0)};
                goals = get_endpoints(landmarks[i].id1, landmarks[i].i1, landmarks[i].j1, landmarks[i].t1, landmarks[i].t2);
            }
            else
            {
                starts.clear();
                for(auto p:results)
                    starts.push_back(p.nodes.back());
                if(i == landmarks.size())
                    goals = {get_endpoints(agent.goal_id, agent.goal_i, agent.goal_j, 0, CN_INFINITY).back()};
                else
                    goals = get_endpoints(landmarks[i].id1, landmarks[i].i1, landmarks[i].j1, landmarks[i].t1, landmarks[i].t2);
            }
            if(goals.empty())
                return Path();
            parts = find_partial_path(starts, goals, map, h_values, goals.back().interval.second);
            new_results.clear();
            if(i == 0)
                for(int i = 0; i < parts.size(); i++)
                {
                    if(parts[i].nodes.empty())
                        continue;
                    new_results.push_back(parts[i]);
                }
            for(int i = 0; i < parts.size(); i++)
                for(int j = 0; j < results.size(); j++)
                {
                    if(parts[i].nodes.empty())
                        continue;
                    if(fabs(parts[i].nodes[0].interval.first - results[j].nodes.back().interval.first) < CN_EPSILON && fabs(parts[i].nodes[0].interval.second - results[j].nodes.back().interval.second) < CN_EPSILON)
                    {
                        new_results.push_back(results[j]);
                        new_results.back() = add_part(new_results.back(), parts[i]);
                    }
                }
            results = new_results;
            if(results.empty())
                return Path();
            if(i < landmarks.size())
            {
                starts.clear();
                for(auto p:results)
                    starts.push_back(p.nodes.back());
                double offset = sqrt(pow(landmarks[i].i1 - landmarks[i].i2, 2) + pow(landmarks[i].j1 - landmarks[i].j2, 2));
                goals = get_endpoints(landmarks[i].id2, landmarks[i].i2, landmarks[i].j2, landmarks[i].t1 + offset, landmarks[i].t2 + offset);
                if(goals.empty())
                    return Path();
                new_results.clear();
                for(int i = 0; i < goals.size(); i++)
                {
                    double best_g(CN_INFINITY);
                    int best_start_id = -1;
                    for(int j = 0; j < starts.size(); j++)
                    {
                        double g = check_endpoint(starts[j], goals[i]);
                        if(g < best_g)
                        {
                            best_start_id = j;
                            best_g = g;
                        }
                    }
                    if(best_start_id >= 0)
                    {
                        goals[i].g = best_g;
                        if(collision_intervals[goals[i].id].empty())
                            goals[i].interval.second = CN_INFINITY;
                        else
                        {
                            for(auto c:collision_intervals[goals[i].id])
                                if(goals[i].g < c.first)
                                {
                                    goals[i].interval.second = c.first;
                                    break;
                                }
                        }
                        new_results.push_back(results[best_start_id]);
                        if(goals[i].g - starts[best_start_id].g > offset + CN_EPSILON)
                        {
                            new_results.back().nodes.push_back(new_results.back().nodes.back());
                            new_results.back().nodes.back().g = goals[i].g - offset;
                        }
                        new_results.back().nodes.push_back(goals[i]);
                    }
                }

                results = new_results;
                if(results.empty())
                    return Path();
            }
        }
        result = results[0];
    }
    else
    {
        starts = {get_endpoints(agent.start_id, agent.start_i, agent.start_j, 0, CN_INFINITY).at(0)};
        goals = {get_endpoints(agent.goal_id, agent.goal_i, agent.goal_j, 0, CN_INFINITY).back()};
        parts = find_partial_path(starts, goals, map, h_values);
        if(parts[0].cost < 0)
            return Path();
        result = parts[0];
    }
    result.cost = result.nodes.back().g;
    result.agentID = agent.id;
    return result;
}
