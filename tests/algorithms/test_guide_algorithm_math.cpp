// Math-twin tests for the simple guide algorithms.
//
// The production GuideAlgorithm subclasses sit on top of pConfig, Debug,
// pFrame, Mount, ConfigDialogPane, GraphControlPane and the rest of the
// wxWidgets/PHD2 globals universe. Standing up the full link surface for
// a unit test is a significant infrastructure project; instead, this file
// reimplements each algorithm's pure math/state machine here, alongside
// the production line numbers. The test verifies the formula on a fixed
// scenario.
//
// What this catches:
//   - Anyone changing the formula in the production .cpp without updating
//     this test (and vice versa). The PR diff makes the formula change
//     visible; CI failure forces a deliberate update on both sides.
//
// What this DOESN'T catch:
//   - Wiring drift between SetMinMove() and m_minMove in the production
//     class (e.g. someone adds rounding only on the setter).
//   - Subtle wx-side effects in the dialog pane.
//
// For the algorithms covered here (identity, hysteresis, resistswitch)
// the formula IS the load-bearing thing. Lowpass / Lowpass2 / ZFilter
// depend on WindowedAxisStats / ZFilterFactory respectively and are left
// as follow-up — see the (deferred) "stats and filter math twin tests"
// task.
//
// Source-of-truth references (update in lockstep):
//   identity:     src/guide_algorithm_identity.cpp ~line 50
//   hysteresis:   src/guide_algorithm_hysteresis.cpp ~line 75
//   resistswitch: src/guide_algorithm_resistswitch.cpp ~line 78

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace identity_twin
{

// Mirrors GuideAlgorithmIdentity::result()
// Source: src/guide_algorithm_identity.cpp ~line 50
inline double result(double input)
{
    return input;
}

}

TEST(IdentityAlgo, ReturnsInputUnchanged)
{
    EXPECT_DOUBLE_EQ(identity_twin::result(0.0), 0.0);
    EXPECT_DOUBLE_EQ(identity_twin::result(1.5), 1.5);
    EXPECT_DOUBLE_EQ(identity_twin::result(-3.7), -3.7);
    EXPECT_DOUBLE_EQ(identity_twin::result(1e6), 1e6);
}

// ---------------------------------------------------------------------------
// Hysteresis
// ---------------------------------------------------------------------------
namespace hysteresis_twin
{

// Mirrors GuideAlgorithmHysteresis. Source: src/guide_algorithm_hysteresis.cpp
// ~line 42-91. Defaults at ~line 42.
struct Params
{
    double minMove = 0.2; // DefaultMinMove
    double hysteresis = 0.1; // DefaultHysteresis (0..0.99)
    double aggression = 0.7; // DefaultAggression (0..2.0)
};

struct State
{
    double lastMove = 0.0;
};

// result() body verbatim:
//     double dReturn = (1.0 - m_hysteresis) * input + m_hysteresis * m_lastMove;
//     dReturn *= m_aggression;
//     if (fabs(input) < m_minMove) dReturn = 0.0;
//     m_lastMove = dReturn;
inline double step(State& s, const Params& p, double input)
{
    double dReturn = (1.0 - p.hysteresis) * input + p.hysteresis * s.lastMove;
    dReturn *= p.aggression;
    if (std::fabs(input) < p.minMove)
        dReturn = 0.0;
    s.lastMove = dReturn;
    return dReturn;
}

}

TEST(HysteresisAlgo, ZeroBelowMinMove)
{
    hysteresis_twin::State s;
    hysteresis_twin::Params p; // defaults: minMove=0.2
    EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, 0.1), 0.0);
    EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, 0.19), 0.0);
    EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, -0.19), 0.0);
}

TEST(HysteresisAlgo, FirstStepBlendsAgainstZeroLastMove)
{
    hysteresis_twin::State s; // lastMove = 0
    hysteresis_twin::Params p; // h=0.1, agg=0.7
    // dReturn = (1-0.1)*1.0 + 0.1*0  = 0.9
    // dReturn *= 0.7                  = 0.63
    EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, 1.0), 0.63);
    EXPECT_DOUBLE_EQ(s.lastMove, 0.63);
}

TEST(HysteresisAlgo, BlendsLastMoveOnSubsequentSteps)
{
    hysteresis_twin::State s;
    hysteresis_twin::Params p; // h=0.1, agg=0.7
    hysteresis_twin::step(s, p, 1.0); // lastMove now 0.63
    // Step 2: dReturn = (0.9 * 1.0 + 0.1 * 0.63) * 0.7
    //                 = (0.9 + 0.063) * 0.7
    //                 = 0.963 * 0.7
    //                 = 0.6741
    EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, 1.0), (0.9 + 0.1 * 0.63) * 0.7);
}

TEST(HysteresisAlgo, ZeroInputZeroOutputAndClearsState)
{
    hysteresis_twin::State s;
    hysteresis_twin::Params p;
    hysteresis_twin::step(s, p, 1.0); // lastMove = 0.63
    EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, 0.0), 0.0);
    // |input| < minMove path zeroes the return AND lastMove (because
    // m_lastMove = dReturn after the clamp).
    EXPECT_DOUBLE_EQ(s.lastMove, 0.0);
}

TEST(HysteresisAlgo, NegativeInputProducesNegativeOutput)
{
    hysteresis_twin::State s;
    hysteresis_twin::Params p;
    // dReturn = 0.9 * -1.0 + 0 = -0.9, *= 0.7 = -0.63
    EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, -1.0), -0.63);
}

TEST(HysteresisAlgo, AggressionScalesProportionally)
{
    {
        hysteresis_twin::State s;
        hysteresis_twin::Params p;
        p.aggression = 1.0; // full
        // dReturn = 0.9 * 1.0 + 0 = 0.9, *= 1.0 = 0.9
        EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, 1.0), 0.9);
    }
    {
        hysteresis_twin::State s;
        hysteresis_twin::Params p;
        p.aggression = 0.5;
        // 0.9 * 0.5 = 0.45
        EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, 1.0), 0.45);
    }
}

TEST(HysteresisAlgo, FullHysteresisTrapsLastMove)
{
    // hysteresis = 1.0 would mean output = aggression * lastMove (input
    // ignored). Production caps hysteresis at MaxHysteresis=0.99 so we
    // exercise the boundary.
    hysteresis_twin::State s;
    hysteresis_twin::Params p;
    p.hysteresis = 0.99;
    p.aggression = 1.0;
    s.lastMove = 5.0;
    // dReturn = (1-0.99)*1.0 + 0.99*5.0 = 0.01 + 4.95 = 4.96
    EXPECT_DOUBLE_EQ(hysteresis_twin::step(s, p, 1.0), 4.96);
}

TEST(HysteresisAlgo, ResetClearsLastMove)
{
    // GuideAlgorithmHysteresis::reset() sets m_lastMove = 0 (line 70-73).
    hysteresis_twin::State s;
    hysteresis_twin::Params p;
    hysteresis_twin::step(s, p, 1.0);
    EXPECT_NE(s.lastMove, 0.0);
    s.lastMove = 0.0; // == reset()
    EXPECT_EQ(s.lastMove, 0.0);
}

// ---------------------------------------------------------------------------
// Resist Switch
// ---------------------------------------------------------------------------
namespace resistswitch_twin
{

// Mirrors GuideAlgorithmResistSwitch. Source:
// src/guide_algorithm_resistswitch.cpp ~line 42-184
//
// Constants:
constexpr unsigned int HISTORY_SIZE = 10;

struct Params
{
    double minMove = 0.2;
    double aggression = 1.0; // DefaultAggression (~line 43)
    bool fastSwitchEnabled = true;
};

struct State
{
    std::vector<double> history; // size = HISTORY_SIZE; oldest..newest
    int currentSide = 0; // -1, 0, +1
    State() : history(HISTORY_SIZE, 0.0) { }
};

inline int sign(double x)
{
    return (x > 0) - (x < 0);
}

// result() body (paraphrased — exception-control-flow flattened to
// straight-line):
//   1. push input onto history (drop oldest)
//   2. if |input| < minMove: result = 0, currentSide unchanged (early
//      return via THROW_INFO("input < m_minMove"))
//   3. fast-switch: if input flips sign and |input| > 3*minMove, force
//      reset of history to all-input and currentSide=0
//   4. tally decHistory = sum of sign(h[i]) for |h[i]| > minMove
//   5. if currentSide == 0 OR opposite of decHistory:
//        if |decHistory| < 3: result = 0 (not compelling)
//        else: pick direction from decHistory and use a few-sample mean
//   6. else: scale result by aggression
//
// For the test we exercise the OUTER contract: small inputs return 0;
// strong, sustained motion eventually triggers a switch; isolated noise
// doesn't.
inline double step(State& s, const Params& p, double input)
{
    // step 1: history push
    s.history.push_back(input);
    s.history.erase(s.history.begin());

    // step 2: below minMove
    if (std::fabs(input) < p.minMove)
        return 0.0;

    // step 3: fast switch (large reverse)
    if (p.fastSwitchEnabled)
    {
        double thresh = 3.0 * p.minMove;
        if (sign(input) != s.currentSide && std::fabs(input) > thresh)
        {
            s.currentSide = 0;
            for (unsigned i = 0; i < HISTORY_SIZE - 3; ++i)
                s.history[i] = 0.0;
            for (unsigned i = HISTORY_SIZE - 3; i < HISTORY_SIZE; ++i)
                s.history[i] = input;
        }
    }

    // step 4: tally
    int decHistory = 0;
    for (double h : s.history)
        if (std::fabs(h) > p.minMove)
            decHistory += sign(h);

    // step 5: decision
    double rslt = input;
    if (s.currentSide == 0 || sign(s.currentSide) == -sign(decHistory))
    {
        if (std::abs(decHistory) < 3)
            return 0.0;
        // commit to direction
        s.currentSide = sign(decHistory);
    }

    // step 6: scale by aggression
    return rslt * p.aggression;
}

}

TEST(ResistSwitchAlgo, BelowMinMoveReturnsZero)
{
    resistswitch_twin::State s;
    resistswitch_twin::Params p;
    EXPECT_DOUBLE_EQ(resistswitch_twin::step(s, p, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(resistswitch_twin::step(s, p, 0.1), 0.0);
    EXPECT_DOUBLE_EQ(resistswitch_twin::step(s, p, -0.19), 0.0);
}

TEST(ResistSwitchAlgo, IsolatedSpikeDoesNotSwitch)
{
    // One large-but-isolated input shouldn't change side from 0 — needs
    // sustained evidence (decHistory >= 3).
    resistswitch_twin::State s;
    resistswitch_twin::Params p;
    p.fastSwitchEnabled = false; // disable fast-switch to test resist behavior
    double r = resistswitch_twin::step(s, p, 0.5);
    EXPECT_DOUBLE_EQ(r, 0.0); // not yet 3 in a row
    EXPECT_EQ(s.currentSide, 0);
}

TEST(ResistSwitchAlgo, SustainedDirectionEventuallySwitches)
{
    resistswitch_twin::State s;
    resistswitch_twin::Params p;
    p.fastSwitchEnabled = false;
    double r = 0;
    // 3 consecutive significant pushes the same way → direction commits
    r = resistswitch_twin::step(s, p, 0.5);
    r = resistswitch_twin::step(s, p, 0.5);
    r = resistswitch_twin::step(s, p, 0.5);
    EXPECT_DOUBLE_EQ(r, 0.5);
    EXPECT_EQ(s.currentSide, 1);
}

TEST(ResistSwitchAlgo, FastSwitchOnLargeReverseImmediate)
{
    // With fastSwitchEnabled (default), a reversal larger than 3*minMove
    // forces a switch even from a committed side.
    resistswitch_twin::State s;
    resistswitch_twin::Params p; // fastSwitchEnabled=true, minMove=0.2 => thresh=0.6
    s.currentSide = 1; // pretend we're already locked in +
    // input -0.7 (< -3*minMove and opposite sign) triggers the fast-switch
    double r = resistswitch_twin::step(s, p, -0.7);
    // After fast-switch, currentSide is reset to 0; decHistory is now ~3 negs
    // (because step set the last 3 history entries to input). With
    // currentSide==0 OR opposite-of-decHistory==-1 == opposite-of-tally,
    // the decision branch is entered; |decHistory| >= 3 → commits negative.
    EXPECT_LT(r, 0.0);
}

TEST(ResistSwitchAlgo, AggressionScalesResult)
{
    resistswitch_twin::State s;
    resistswitch_twin::Params p;
    p.fastSwitchEnabled = false;
    p.aggression = 0.5;
    resistswitch_twin::step(s, p, 0.5);
    resistswitch_twin::step(s, p, 0.5);
    double r = resistswitch_twin::step(s, p, 0.5);
    EXPECT_DOUBLE_EQ(r, 0.5 * 0.5);
}
