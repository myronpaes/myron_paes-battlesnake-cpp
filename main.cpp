// Build your snake:
// docker run -d --network snakenet --name myron myron

// Test your snake:
// docker run --rm --network snakenet battlesnake_board battlesnake play -v --name myron --url http://myron:8000 --name myron2 --url http://myron:8000 --name myron3 --url http://myron:8000 --name myron4 --url http://myron:8000

#include "httplib.h"
#include "json.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <chrono>
#include <queue>
#include <algorithm>
#include <memory>
#include <mutex>

using json = nlohmann::json;
using namespace std;

// Strict Math Helper
double strict_round_5(double val)
{
    return std::floor(val * 100000.0 + 0.5) / 100000.0;
}

// Spatial Helpers

struct Point
{
    int x, y;
    bool operator==(const Point &o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point &o) const { return !(*this == o); }
};

namespace std
{
    template <>
    struct hash<Point>
    {
        size_t operator()(const Point &p) const { return hash<int>()(p.x) ^ (hash<int>()(p.y) << 1); }
    };
}

inline int to1D(int x, int y, int width) { return y * width + x; }

// Game State Definitions

struct Snake
{
    string id;
    Point head;
    vector<Point> body;
    int length;
    int health;
};

class GameState
{
public:
    int width, height;
    vector<Point> food;
    unordered_map<string, Snake> snakes;
    string my_id;

    GameState() = default;

    // JSON Parser Constructor
    GameState(const json &data, string myId)
    {
        width = data["board"]["width"];
        height = data["board"]["height"];
        my_id = myId;

        for (auto &f : data["board"]["food"])
        {
            food.push_back({f["x"], f["y"]});
        }

        for (auto &s : data["board"]["snakes"])
        {
            Snake snake;
            snake.id = s["id"];
            snake.head = {s["head"]["x"], s["head"]["y"]};
            snake.health = s["health"];
            snake.length = s["length"];
            for (auto &b : s["body"])
            {
                snake.body.push_back({b["x"], b["y"]});
            }
            snakes[snake.id] = snake;
        }
    }

    // Flood fill
    int flood_fill_size(Point start, const vector<bool> &occupied) const
    {
        if (occupied[to1D(start.x, start.y, width)])
            return 0;

        vector<bool> visited = occupied;
        vector<Point> stack;
        stack.push_back(start);
        visited[to1D(start.x, start.y, width)] = true;

        int count = 0;
        int dx[] = {0, 0, -1, 1};
        int dy[] = {1, -1, 0, 0};

        while (!stack.empty())
        {
            Point p = stack.back();
            stack.pop_back();
            count++;

            for (int i = 0; i < 4; i++)
            {
                int nx = p.x + dx[i];
                int ny = p.y + dy[i];
                if (nx >= 0 && nx < width && ny >= 0 && ny < height)
                {
                    int idx = to1D(nx, ny, width);
                    if (!visited[idx])
                    {
                        visited[idx] = true;
                        stack.push_back({nx, ny});
                    }
                }
            }
        }
        return count;
    }

    vector<string> get_legal_moves(string snake_id, bool strict_threats = true) const
    {
        if (snakes.find(snake_id) == snakes.end())
            return {};

        const Snake &snake = snakes.at(snake_id);
        Point head = snake.head;
        int my_length = snake.length;

        vector<pair<string, Point>> dirs = {
            {"up", {head.x, head.y + 1}}, {"down", {head.x, head.y - 1}}, {"left", {head.x - 1, head.y}}, {"right", {head.x + 1, head.y}}};

        vector<bool> occupied(width * height, false);
        for (auto &kv : snakes)
        {
            for (size_t i = 0; i < kv.second.body.size() - 1; i++)
            {
                Point p = kv.second.body[i];
                occupied[to1D(p.x, p.y, width)] = true;
            }
        }

        vector<bool> step1_threats(width * height, false);
        for (auto &kv : snakes)
        {
            if (kv.first != snake_id && kv.second.length >= my_length)
            {
                Point oh = kv.second.head;
                int dx[] = {0, 0, -1, 1};
                int dy[] = {1, -1, 0, 0};
                for (int i = 0; i < 4; i++)
                {
                    int nx = oh.x + dx[i], ny = oh.y + dy[i];
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height)
                    {
                        step1_threats[to1D(nx, ny, width)] = true;
                    }
                }
            }
        }

        vector<string> absolute_safe;
        vector<string> clear_moves;

        for (auto &d : dirs)
        {
            Point p = d.second;
            if (p.x >= 0 && p.x < width && p.y >= 0 && p.y < height && !occupied[to1D(p.x, p.y, width)])
            {
                clear_moves.push_back(d.first);
                if (!step1_threats[to1D(p.x, p.y, width)])
                {
                    absolute_safe.push_back(d.first);
                }
            }
        }

        if (strict_threats)
            return absolute_safe;
        return absolute_safe.empty() ? clear_moves : absolute_safe;
    }

    bool is_terminal() const
    {
        return snakes.find(my_id) == snakes.end() || snakes.size() <= 1;
    }

    GameState generate_next_state(string my_move) const
    {
        GameState next_state = *this;
        unordered_map<string, string> moves;
        moves[my_id] = my_move;

        int my_length = 0;
        if (snakes.find(my_id) != snakes.end())
            my_length = snakes.at(my_id).length;

        for (auto &kv : snakes)
        {
            if (kv.first == my_id)
                continue;
            vector<string> safe_moves = get_legal_moves(kv.first, false);
            if (safe_moves.empty())
            {
                moves[kv.first] = "down";
                continue;
            }
            // Simplified random choice for C++ simulation speed
            moves[kv.first] = safe_moves[rand() % safe_moves.size()];
        }

        unordered_set<string> fed_snakes;
        unordered_map<string, Point> new_heads;

        for (auto &kv : moves)
        {
            if (next_state.snakes.find(kv.first) == next_state.snakes.end())
                continue;
            Snake &s = next_state.snakes[kv.first];
            Point nh = s.head;
            if (kv.second == "up")
                nh.y++;
            else if (kv.second == "down")
                nh.y--;
            else if (kv.second == "left")
                nh.x--;
            else if (kv.second == "right")
                nh.x++;

            s.body.insert(s.body.begin(), nh);
            s.head = nh;
            s.health--;

            auto f_it = find(next_state.food.begin(), next_state.food.end(), nh);
            if (f_it != next_state.food.end())
            {
                s.health = 100;
                s.length++;
                fed_snakes.insert(kv.first);
                next_state.food.erase(f_it);
            }
            new_heads[kv.first] = nh;
        }

        for (auto &kv : next_state.snakes)
        {
            if (fed_snakes.find(kv.first) == fed_snakes.end() && !kv.second.body.empty())
            {
                kv.second.body.pop_back();
            }
        }

        unordered_set<string> eliminated;
        vector<string> body_cells(width * height, "");
        for (auto &kv : next_state.snakes)
        {
            for (size_t i = 1; i < kv.second.body.size(); i++)
            {
                Point p = kv.second.body[i];
                body_cells[to1D(p.x, p.y, width)] = kv.first;
            }
        }

        for (auto &kv : next_state.snakes)
        {
            Point h = kv.second.head;
            if (kv.second.health <= 0 || h.x < 0 || h.x >= width || h.y < 0 || h.y >= height)
            {
                eliminated.insert(kv.first);
                continue;
            }
            string occupant = body_cells[to1D(h.x, h.y, width)];
            if (occupant != "" && occupant != kv.first)
            {
                eliminated.insert(kv.first);
            }
        }

        for (auto e : eliminated)
            next_state.snakes.erase(e);
        return next_state;
    }

    double evaluate_voronoi() const
    {
        if (snakes.find(my_id) == snakes.end())
            return 0.0;

        vector<string> claimed(width * height, "");
        deque<tuple<int, int, string>> q;
        const Snake &my_snake = snakes.at(my_id);

        for (auto &kv : snakes)
        {
            q.push_back({kv.second.head.x, kv.second.head.y, kv.first});
            claimed[to1D(kv.second.head.x, kv.second.head.y, width)] = kv.first;
            for (auto &p : kv.second.body)
            {
                claimed[to1D(p.x, p.y, width)] = "body";
            }
        }

        int dx[] = {0, 0, -1, 1};
        int dy[] = {1, -1, 0, 0};

        while (!q.empty())
        {
            auto [x, y, owner] = q.front();
            q.pop_front();

            for (int i = 0; i < 4; i++)
            {
                int nx = x + dx[i];
                int ny = y + dy[i];
                if (nx >= 0 && nx < width && ny >= 0 && ny < height)
                {
                    if (claimed[to1D(nx, ny, width)] == "")
                    {
                        claimed[to1D(nx, ny, width)] = owner;
                        q.push_back({nx, ny, owner});
                    }
                }
            }
        }

        int my_territory = 0;
        for (const string &owner : claimed)
        {
            if (owner == my_id)
                my_territory++;
        }

        double score = (double)my_territory / (width * height);
        return strict_round_5(max(0.0, min(1.0, score)));
    }
};

// Monte Carlo Tree Search Agent

struct MCTSNode
{
    GameState state;
    MCTSNode *parent;
    string move_that_led_here;
    vector<MCTSNode *> children;
    int visits;
    double wins;
    vector<string> untried_moves;

    MCTSNode(GameState st, MCTSNode *p = nullptr, string mv = "") : state(st), parent(p), move_that_led_here(mv), visits(0), wins(0.0)
    {
        untried_moves = state.get_legal_moves(state.my_id);
    }

    ~MCTSNode()
    {
        for (auto child : children)
            delete child;
    }

    double calculate_ucb1() const
    {
        if (visits == 0)
            return 999999.0;
        double exploit = wins / visits;
        double explore = 1.41421 * sqrt(log(parent->visits) / visits);
        return strict_round_5(exploit + explore);
    }
};

class MCTSAgent
{
public:
    MCTSNode *last_root = nullptr;

    ~MCTSAgent()
    {
        if (last_root)
            delete last_root;
    }

    string get_best_move(GameState &root_state, auto start_time, int time_limit_ms = 350)
    {
        MCTSNode *root_node = new MCTSNode(root_state);

        while (chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start_time).count() < time_limit_ms)
        {
            MCTSNode *node = root_node;

            while (node->untried_moves.empty() && !node->children.empty())
            {
                auto best_it = max_element(node->children.begin(), node->children.end(),
                                           [](MCTSNode *a, MCTSNode *b)
                                           { return a->calculate_ucb1() < b->calculate_ucb1(); });
                node = *best_it;
            }

            if (!node->untried_moves.empty() && !node->state.is_terminal())
            {
                string move = node->untried_moves.back();
                node->untried_moves.pop_back();
                GameState next_state = node->state.generate_next_state(move);
                MCTSNode *child = new MCTSNode(next_state, node, move);
                node->children.push_back(child);
                node = child;
            }

            double score = node->state.evaluate_voronoi();

            while (node != nullptr)
            {
                node->visits++;
                node->wins += score;
                node = node->parent;
            }
        }

        string best_move = "down";
        if (!root_node->children.empty())
        {
            auto best_it = max_element(root_node->children.begin(), root_node->children.end(),
                                       [](MCTSNode *a, MCTSNode *b)
                                       { return a->visits < b->visits; });
            best_move = (*best_it)->move_that_led_here;
        }
        else
        {
            vector<string> safe = root_state.get_legal_moves(root_state.my_id);
            if (!safe.empty())
                best_move = safe[0];
        }

        if (last_root)
            delete last_root;
        last_root = root_node;

        return best_move;
    }
};

// Web Server Execution

// Thread-safe global memory storage
std::mutex agent_mutex;
std::unordered_map<std::string, std::unique_ptr<MCTSAgent>> active_agents;

int main()
{
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request &, httplib::Response &res)
            {
        json info = {
            {"apiversion", "1"},
            {"author", "Myron"},
            {"color", "#099347"},
            {"head", "do-sammy"},
            {"tail", "do-sammy"}
        };
        res.set_content(info.dump(), "application/json"); });

    svr.Post("/start", [](const httplib::Request &, httplib::Response &res)
             {
        cout << "GAME START\n";
        res.set_content("ok", "text/plain"); });

    svr.Post("/move", [](const httplib::Request &req, httplib::Response &res)
             {
        auto start_time = chrono::steady_clock::now();
        json data = json::parse(req.body);
        
        string game_id = data["game"]["id"];
        string my_id = data["you"]["id"];
        string agent_key = game_id + "_" + my_id; // Unique memory tree for EACH snake

        GameState state(data, my_id);
        vector<string> absolute_safe = state.get_legal_moves(state.my_id, true);

        if (absolute_safe.size() == 1) {
            json resp = {{"move", absolute_safe[0]}};
            res.set_content(resp.dump(), "application/json");
            return;
        }

        // Safely retrieve or create this specific snake's memory tree
        agent_mutex.lock();
        if (active_agents.find(agent_key) == active_agents.end()) {
            active_agents[agent_key] = std::make_unique<MCTSAgent>();
        }
        MCTSAgent* agent = active_agents[agent_key].get();
        agent_mutex.unlock();

        // RUN MCTS (Set to 80ms for 4 snake local testing, increase to 350ms later)
        string next_move = agent->get_best_move(state, start_time, 350); 

        // Veto System
        if (find(absolute_safe.begin(), absolute_safe.end(), next_move) == absolute_safe.end()) {
            if (!absolute_safe.empty()) next_move = absolute_safe[0];
            else {
                vector<string> fallback = state.get_legal_moves(state.my_id, false);
                next_move = fallback.empty() ? "down" : fallback[0];
            }
        }

        cout << "MOVE " << data["turn"] << " (" << my_id << "): " << next_move << "\n";
        json resp = {{"move", next_move}};
        res.set_content(resp.dump(), "application/json"); });

    svr.Post("/end", [](const httplib::Request &req, httplib::Response &res)
             {
        json data = json::parse(req.body);
        string game_id = data["game"]["id"];
        
        // Clean up game memory
        agent_mutex.lock();
        for (auto it = active_agents.begin(); it != active_agents.end(); ) {
            if (it->first.find(game_id) == 0) {
                it = active_agents.erase(it);
            } else {
                ++it;
            }
        }
        agent_mutex.unlock();

        cout << "GAME OVER\n";
        res.set_content("ok", "text/plain"); });

    cout << "Starting Thread-Safe C++ Battlesnake Server on port 8000...\n";
    svr.listen("0.0.0.0", 8000);
    return 0;
}