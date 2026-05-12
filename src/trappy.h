/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TRAPPY_H_INCLUDED
#define TRAPPY_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>

#include "types.h"

namespace Stockfish {

class OptionsMap;

namespace Trappy {

enum class Assessment {
    Median,
    Best,
    Last
};

struct Config {
    bool       enabled      = true;
    int        maxPly       = 5;
    Assessment assessment   = Assessment::Median;
    Value      maxSacrifice = 200;
    Value      bonusCap     = 300;
    Value      minProfit    = 50;
    bool       trace        = false;
};

struct Result {
    bool  applied            = false;
    Move  trapMove           = Move::none();
    Move  trapReply          = Move::none();
    Value normalScore        = -VALUE_INFINITE;
    Value adjustedScore      = -VALUE_INFINITE;
    Value replyFinalScore    = VALUE_ZERO;
    Value profit             = VALUE_ZERO;
    int   trappinessPermille = 0;
    int   quality            = 0;
    int   bonus              = 0;
};

Config make_config(const OptionsMap& options);

class Context {
   public:
    void clear();
    void start_search(const Config& cfg, Color rootSide);
    void start_iteration(Depth depth);

    bool enabled() const { return config.enabled; }
    bool trace_enabled() const { return config.enabled && config.trace; }
    bool collecting_replies(Color sideToMove, int ply) const;
    bool trap_setting_node(Color sideToMove, int ply) const;

    void record_reply(Key parentKey, Move reply, int ply, Value score);

    Result adjust_move(Key   replyNodeKey,
                       Move  trapMove,
                       int   replyPly,
                       Value normalScore,
                       Value bestNormalScore) const;

   private:
    struct DepthScore {
        Depth depth;
        Value score;
    };

    struct ReplyLine {
        Move                    reply = Move::none();
        std::vector<DepthScore> scores;
    };

    struct NodeKey {
        Key key = 0;
        int ply = 0;

        bool operator==(const NodeKey& other) const { return key == other.key && ply == other.ply; }
    };

    struct NodeKeyHash {
        std::size_t operator()(const NodeKey& k) const {
            return std::size_t(k.key ^ (k.key >> 32) ^ (uint64_t(k.ply) * 0x9e3779b97f4a7c15ULL));
        }
    };

    using ReplyTable = std::unordered_map<NodeKey, std::vector<ReplyLine>, NodeKeyHash>;

    int aggregate_shallow(const std::vector<Value>& scores) const;
    int trappiness(Value finalScore, const std::vector<Value>& shallowScores, int ply) const;
    int scale_quality(int quality, Value normalScore) const;

    Config     config;
    Color      rootSide     = WHITE;
    Depth      currentDepth = 0;
    ReplyTable replyTable;
};

}  // namespace Trappy
}  // namespace Stockfish

#endif  // #ifndef TRAPPY_H_INCLUDED
