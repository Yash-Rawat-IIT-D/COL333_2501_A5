#pragma once

#include <vector>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <cmath>

#include "constants.hpp"
#include "piece.hpp"
#include "move.hpp"
#include "state.hpp"
#include "movegen.hpp"

/*
 * ==================== BOARD EVALUATION ENGINE ====================
 * C++11-compatible, global namespace.
 */

class BoardEvaluator {
private:
    MoveGenerator* moveGenerator;

    struct ScoringArea {
        int row;
        std::vector<int> score_cols;
        ScoringArea() : row(0) {}
    };

    struct RiverFlowCacheEntry {
        int river_x, river_y;
        int source_x, source_y;
        bool isCirclePlayer;
        std::vector<std::pair<int,int> > flow_destinations;

        RiverFlowCacheEntry(int rx, int ry, int sx, int sy, bool isCircle,
                            const std::vector<std::pair<int,int> >& destinations)
            : river_x(rx), river_y(ry), source_x(sx), source_y(sy),
              isCirclePlayer(isCircle), flow_destinations(destinations) {}
    };

    struct RiverSimulationCacheEntry {
        int river_x, river_y;
        bool isCirclePlayer;
        std::vector<std::pair<int,int> > simulation_result;

        RiverSimulationCacheEntry(int rx, int ry, bool isCircle,
                                  const std::vector<std::pair<int,int> >& result)
            : river_x(rx), river_y(ry), isCirclePlayer(isCircle), simulation_result(result) {}
    };

    bool defense_mode;
    mutable bool river_flows_computed;
    mutable bool scoring_areas_initialized;
    mutable uint64_t cached_board_hash;

    mutable ScoringArea circle_scoring;
    mutable ScoringArea square_scoring;

    mutable std::vector<std::vector<std::vector<std::vector<std::pair<int,int> > > > > river_flow_cache;

    float weight1;
    float weight2;
    float weight_river_mobility;
    float weight_river_combos;
    float weight_board_adv;
    float weight_defense;

    void initializeScoringAreas(const GameState& gameState) const;

    // Internal helpers
    float computeBasicEvaluation(const GameState& gameState, bool isCirclePlayer) const;
    float evaluatePosition(const GameState& gameState, bool isCirclePlayer) const;
    float evaluateMobility(const GameState& gameState, bool isCirclePlayer) const;

    float evaluateDefense(const GameState& gameState, bool isCirclePlayer) const;
    float evaluateColumnDefense(const GameState& gameState, int score_col,
                                int opponent_scoring_row, bool isCirclePlayer) const;
    float evaluateDefensePosition(const GameState& gameState, int x, int y,
                                  bool isCirclePlayer, const std::string& position_type) const;
    float evaluateBoardAdvancement(const GameState& gameState, bool isCirclePlayer) const;
    float evaluateRiverMobility(const GameState& gameState, bool isCirclePlayer,
                                std::vector<RiverSimulationCacheEntry>& cache) const;
    float evaluateRiverCombos(const GameState& gameState, bool isCirclePlayer,
                              std::vector<RiverSimulationCacheEntry>& cache) const;

    float evaluateRiverConnectionWithCache(const GameState& gameState,
                                           const std::pair<int,int>& river1,
                                           const std::pair<int,int>& river2,
                                           bool isCirclePlayer,
                                           std::vector<RiverSimulationCacheEntry>& cache) const;

    std::vector<std::pair<int,int> > simulateRiverFlow(const GameState& gameState,
                                                      int start_x, int start_y,
                                                      bool isCirclePlayer) const;

    std::vector<std::pair<int,int> > getCachedRiverSimulation(
        const GameState& gameState, int river_x, int river_y, bool isCirclePlayer,
        std::vector<RiverSimulationCacheEntry>& cache) const;

    float countReachableOpponentPositionsWithCache(const GameState& gameState, int river_x, int river_y,
                                                   bool isCirclePlayer,
                                                   std::vector<RiverSimulationCacheEntry>& cache) const;

    float countReachableScoringPositionsWithCache(const GameState& gameState, int river_x, int river_y,
                                                  bool isCirclePlayer,
                                                  std::vector<RiverSimulationCacheEntry>& cache) const;

    std::vector<std::pair<int,int> > getCachedRiverFlow(
        const GameState& gameState, int river_x, int river_y, int source_x, int source_y,
        bool isCirclePlayer, std::vector<RiverFlowCacheEntry>& cache,
        int& cache_hits, int& cache_misses) const;

    std::vector<int> getmoveDistancesFromScoringArea(const GameState& gameState, bool isCirclePlayer) const;
    int distance_from_piece_with_cache(const GameState& gameState, bool isCirclePlayer,
                                       const std::pair<int,int>& piece,
                                       std::vector<RiverFlowCacheEntry>& river_flow_cache) const;

    int countStonesInScoringArea(const GameState& gameState, bool isCirclePlayer) const;
    int countRiversInScoringArea(const GameState& gameState, bool isCirclePlayer) const;
    int countStonesAdjacentToRivers(const GameState& gameState, bool isCirclePlayer) const;

public:
    BoardEvaluator(MoveGenerator* moveGen = 0);

    void setMoveGenerator(MoveGenerator* moveGen);
    void setDefenseMode(bool mode);

    float EvaluateBoard(const GameState& gameState, bool isCirclePlayer) const;
};
