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

#include "trappy.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "ucioption.h"

namespace Stockfish::Trappy {

Config make_config(const OptionsMap& options) {
    Config cfg;

    cfg.enabled      = bool(options["Trappy Minimax"]);
    cfg.maxPly       = int(options["Trappy Max Ply"]);
    cfg.maxSacrifice = Value(int(options["Trappy Max Sacrifice"]));
    cfg.bonusCap     = Value(int(options["Trappy Bonus Cap"]));
    cfg.minProfit    = Value(int(options["Trappy Min Profit"]));
    cfg.trace        = bool(options["Trappy Trace"]);

    const std::string assessment = options["Trappy Assessment"] == "best" ? "best"
                                 : options["Trappy Assessment"] == "last" ? "last"
                                                                          : "median";

    if (assessment == "best")
        cfg.assessment = Assessment::Best;
    else if (assessment == "last")
        cfg.assessment = Assessment::Last;
    else
        cfg.assessment = Assessment::Median;

    return cfg;
}

void Context::clear() { replyTable.clear(); }

void Context::start_search(const Config& cfg, Color rootSide_) {
    config       = cfg;
    rootSide     = rootSide_;
    currentDepth = 0;
    clear();
}

void Context::start_iteration(Depth depth) { currentDepth = depth; }

bool Context::collecting_replies(Color sideToMove, int ply) const {
    return config.enabled && currentDepth >= 2 && sideToMove != rootSide && ply <= config.maxPly;
}

bool Context::trap_setting_node(Color sideToMove, int ply) const {
    return config.enabled && currentDepth >= 3 && sideToMove == rootSide && ply < config.maxPly;
}

void Context::record_reply(Key parentKey, Move reply, int ply, Value score) {
    if (!config.enabled || currentDepth <= 0 || !reply.is_ok() || is_decisive(score))
        return;

    NodeKey key{parentKey, ply};
    auto&   lines = replyTable[key];
    auto    it    = std::find_if(lines.begin(), lines.end(),
                                 [reply](const ReplyLine& line) { return line.reply == reply; });

    if (it == lines.end())
    {
        lines.emplace_back();
        it        = lines.end() - 1;
        it->reply = reply;
    }

    auto scoreIt = std::find_if(it->scores.begin(), it->scores.end(),
                                [this](const DepthScore& ds) { return ds.depth == currentDepth; });

    if (scoreIt == it->scores.end())
        it->scores.push_back({currentDepth, score});
    else
        scoreIt->score = score;
}

int Context::aggregate_shallow(const std::vector<Value>& scores) const {
    assert(!scores.empty());

    if (config.assessment == Assessment::Best)
        return *std::max_element(scores.begin(), scores.end());

    if (config.assessment == Assessment::Last)
        return scores.back();

    std::vector<Value> sorted = scores;
    std::sort(sorted.begin(), sorted.end());
    return sorted[sorted.size() / 2];
}

int Context::trappiness(Value finalScore, const std::vector<Value>& shallowScores, int ply) const {
    if (shallowScores.empty())
        return 0;

    int inner    = aggregate_shallow(shallowScores);
    int distance = 0;
    for (int i = int(shallowScores.size()) - 1; i >= 0; --i)
    {
        if (shallowScores[size_t(i)] == inner)
        {
            distance = i;
            break;
        }
    }

    if (distance > int(shallowScores.size()) / 2)
        inner += distance - int(shallowScores.size()) / 2;

    const int last = int(finalScore);
    const int base = std::max(std::abs(last), 1);

    if (ply % 2 == 1)
    {
        if (inner <= last)
            return 0;
        if (inner < last + base)
            return 750 * (inner - last) / base;
        if (inner < last + 4 * base)
            return 750 + 250 * (inner - last - base) / (3 * base);
        return 1000;
    }

    if (inner >= last)
        return 0;
    if (inner > last - base)
        return 750 * (last - inner) / base;
    if (inner > last - 4 * base)
        return 750 + 250 * (last - inner - base) / (3 * base);
    return 1000;
}

int Context::scale_quality(int quality, Value normalScore) const {
    if (quality <= 0)
        return 0;

    const int m = std::max(std::abs(int(normalScore)), 1);
    const int linearlyScaled =
      quality >= 2 * m ? config.bonusCap : quality * int(config.bonusCap) / (2 * m);

    return std::clamp(linearlyScaled, 0, int(config.bonusCap));
}

Result Context::adjust_move(
  Key replyNodeKey, Move trapMove, int replyPly, Value normalScore, Value bestNormalScore) const {
    Result best;
    best.trapMove      = trapMove;
    best.normalScore   = normalScore;
    best.adjustedScore = normalScore;

    if (!config.enabled || !trapMove.is_ok() || is_decisive(normalScore)
        || is_decisive(bestNormalScore) || bestNormalScore - normalScore > config.maxSacrifice)
        return best;

    auto node = replyTable.find(NodeKey{replyNodeKey, replyPly});
    if (node == replyTable.end())
        return best;

    for (const ReplyLine& line : node->second)
    {
        if (line.scores.size() < 2)
            continue;

        std::vector<DepthScore> ordered = line.scores;
        std::sort(ordered.begin(), ordered.end(),
                  [](const DepthScore& a, const DepthScore& b) { return a.depth < b.depth; });

        const Value finalOpponentScore = ordered.back().score;
        if (is_decisive(finalOpponentScore))
            continue;

        std::vector<Value> shallow;
        shallow.reserve(ordered.size() - 1);
        for (size_t i = 0; i + 1 < ordered.size(); ++i)
            shallow.push_back(ordered[i].score);

        const Value rootScoreAfterReply = -finalOpponentScore;
        const Value profit              = rootScoreAfterReply - normalScore;
        if (profit < config.minProfit)
            continue;

        const int factor  = trappiness(finalOpponentScore, shallow, replyPly);
        const int quality = int(profit) * factor / 1000;
        const int bonus   = scale_quality(quality, normalScore);
        if (bonus <= best.bonus)
            continue;

        best.applied         = true;
        best.trapReply       = line.reply;
        best.adjustedScore   = std::clamp<Value>(normalScore + bonus, VALUE_TB_LOSS_IN_MAX_PLY + 1,
                                                 VALUE_TB_WIN_IN_MAX_PLY - 1);
        best.replyFinalScore = finalOpponentScore;
        best.profit          = profit;
        best.trappinessPermille = factor;
        best.quality            = quality;
        best.bonus              = bonus;
    }

    return best;
}

}  // namespace Stockfish::Trappy
