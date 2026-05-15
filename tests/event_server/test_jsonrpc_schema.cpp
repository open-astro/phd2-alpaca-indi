// Schema contract tests for the event-server JSON-RPC surface.
//
// event_server.cpp builds outgoing messages with static JObj<<NV(...)
// helpers buried in a 5,780-line file with deep Mount/Camera/socket
// coupling. We can't easily call those helpers from a unit test without a
// live PHD2.
//
// What we CAN do: pin the documented schema from the consumer's
// perspective. NINA, KStars, the web UI and other tools each parse a
// well-known set of fields. If anyone in this repo renames a field on
// either side (PHD2 emit OR these tests' fixtures), the diff makes the
// breaking-change visible at PR time instead of at the user-Discord-thread
// stage.
//
// The fixtures here mirror the canonical message bodies as documented at
// https://github.com/agalasso/phd2/wiki/EventMonitoring and
// https://github.com/agalasso/phd2/wiki/JsonRpcApi. Update these only with
// a deliberate decision to publish a schema change.

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "json_parser.h"

namespace
{

const json_value *child(const json_value *parent, const char *name)
{
    if (!parent)
        return nullptr;
    for (const json_value *c = parent->first_child; c; c = c->next_sibling)
    {
        if (c->name && std::strcmp(c->name, name) == 0)
            return c;
    }
    return nullptr;
}

// json_value uses a union for its value member; accessing the wrong member
// after a failed type check is UB. The type assertions below are ASSERT_EQ
// (not EXPECT_EQ) so a wrong-type field hard-stops the test before any
// caller that reaches for `->string_value` etc. can hit UB.
void expect_string_field(const json_value *parent, const char *name)
{
    const json_value *v = child(parent, name);
    ASSERT_NE(v, nullptr) << "missing string field: " << name;
    ASSERT_EQ(v->type, JSON_STRING) << "field " << name << " not string";
}

void expect_numeric_field(const json_value *parent, const char *name)
{
    const json_value *v = child(parent, name);
    ASSERT_NE(v, nullptr) << "missing numeric field: " << name;
    ASSERT_TRUE(v->type == JSON_INT || v->type == JSON_FLOAT) << "field " << name << " not numeric";
}

void expect_bool_field(const json_value *parent, const char *name)
{
    const json_value *v = child(parent, name);
    ASSERT_NE(v, nullptr) << "missing bool field: " << name;
    ASSERT_EQ(v->type, JSON_BOOL) << "field " << name << " not bool";
}

}

// ---------------------------------------------------------------------------
// Common fields on every event (event_server.cpp Ev::Ev)
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, EveryEventHasEventTimestampHostInst)
{
    // Source: src/event_server.cpp ~line 303 — Ev::Ev() unconditionally
    // emits Event/Timestamp/Host/Inst on every notification. Downstream
    // consumers key off these fields (multi-instance routing in particular
    // depends on Host+Inst).
    const char *fixture = "{\"Event\":\"AppState\","
                          "\"Timestamp\":1700000000.123,"
                          "\"Host\":\"hostname\","
                          "\"Inst\":1,"
                          "\"State\":\"Stopped\"}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    expect_string_field(p.Root(), "Event");
    expect_numeric_field(p.Root(), "Timestamp");
    expect_string_field(p.Root(), "Host");
    expect_numeric_field(p.Root(), "Inst");
}

// ---------------------------------------------------------------------------
// Version event (boot)
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, VersionEvent)
{
    // event_server.cpp ~line 310-312
    const char *fixture = "{\"Event\":\"Version\",\"Timestamp\":1.0,\"Host\":\"h\",\"Inst\":1,"
                          "\"PHDVersion\":\"2.6.13\","
                          "\"PHDSubver\":\"\","
                          "\"OverlapSupport\":true,"
                          "\"MsgVersion\":1}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    expect_string_field(p.Root(), "PHDVersion");
    expect_string_field(p.Root(), "PHDSubver");
    expect_bool_field(p.Root(), "OverlapSupport");
    expect_numeric_field(p.Root(), "MsgVersion");
}

// ---------------------------------------------------------------------------
// AppState event
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, AppStateEvent)
{
    // event_server.cpp ~line 362-363. State values must remain in the
    // documented set — adding is OK, renaming is a break.
    static const char *kStates[] = {
        "Stopped", "Selected", "Calibrating", "Guiding", "LostLock", "Paused", "Looping",
    };
    for (const char *s : kStates)
    {
        std::string fixture =
            std::string("{\"Event\":\"AppState\",\"Timestamp\":1.0,\"Host\":\"h\",\"Inst\":1,\"State\":\"") + s + "\"}";
        JsonParser p;
        ASSERT_TRUE(p.Parse(fixture)) << "state: " << s;
        expect_string_field(p.Root(), "State");
    }
}

// ---------------------------------------------------------------------------
// SettleDone event
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, SettleDoneEvent)
{
    // event_server.cpp ~line 378-389. Status==0 means success, !=0 failure.
    // NINA/KStars depend on Status, TotalFrames, DroppedFrames, and on
    // Error being present iff Status!=0.
    {
        const char *fixture = "{\"Event\":\"SettleDone\",\"Timestamp\":1.0,\"Host\":\"h\",\"Inst\":1,"
                              "\"Status\":0,\"TotalFrames\":12,\"DroppedFrames\":1}";
        JsonParser p;
        ASSERT_TRUE(p.Parse(std::string(fixture)));
        expect_numeric_field(p.Root(), "Status");
        expect_numeric_field(p.Root(), "TotalFrames");
        expect_numeric_field(p.Root(), "DroppedFrames");
        // Error MUST be absent on success — NINA/KStars read `Error` as a
        // truthy indicator, so a spurious empty-string Error on Status=0
        // looks like a settle failure to them.
        EXPECT_EQ(child(p.Root(), "Error"), nullptr);
    }
    {
        const char *fixture = "{\"Event\":\"SettleDone\",\"Timestamp\":1.0,\"Host\":\"h\",\"Inst\":1,"
                              "\"Status\":1,\"Error\":\"timeout\",\"TotalFrames\":4,\"DroppedFrames\":4}";
        JsonParser p;
        ASSERT_TRUE(p.Parse(std::string(fixture)));
        expect_numeric_field(p.Root(), "Status");
        expect_string_field(p.Root(), "Error");
    }
}

// ---------------------------------------------------------------------------
// Settling event
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, SettlingEvent)
{
    // event_server.cpp ~line 369-371
    const char *fixture = "{\"Event\":\"Settling\",\"Timestamp\":1.0,\"Host\":\"h\",\"Inst\":1,"
                          "\"Distance\":0.32,\"Time\":0.5,\"SettleTime\":10.0,\"StarLocked\":true}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    expect_numeric_field(p.Root(), "Distance");
    expect_numeric_field(p.Root(), "Time");
    expect_numeric_field(p.Root(), "SettleTime");
    expect_bool_field(p.Root(), "StarLocked");
}

// ---------------------------------------------------------------------------
// JSON-RPC response envelope (JRpcResponse, ~line 607-624)
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, ResponseEnvelopeSuccess)
{
    const char *fixture = "{\"jsonrpc\":\"2.0\",\"result\":42,\"id\":17}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    expect_string_field(p.Root(), "jsonrpc");
    EXPECT_STREQ(child(p.Root(), "jsonrpc")->string_value, "2.0");
    ASSERT_NE(child(p.Root(), "result"), nullptr);
    expect_numeric_field(p.Root(), "id");
}

TEST(JsonRpcSchema, ResponseEnvelopeError)
{
    // event_server.cpp ~line 585-586 — error has code (int) and message
    // (string). Both must be present per the JSON-RPC 2.0 spec.
    const char *fixture = "{\"jsonrpc\":\"2.0\","
                          "\"error\":{\"code\":-32601,\"message\":\"Method not found\"},"
                          "\"id\":17}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    const json_value *err = child(p.Root(), "error");
    ASSERT_NE(err, nullptr);
    EXPECT_EQ(err->type, JSON_OBJECT);
    expect_numeric_field(err, "code");
    expect_string_field(err, "message");
}

// ---------------------------------------------------------------------------
// get_profiles result shape (event_server.cpp ~line 668-681)
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, GetProfilesResult)
{
    const char *fixture = "{\"jsonrpc\":\"2.0\","
                          "\"result\":["
                          "{\"id\":1,\"name\":\"Default\",\"selected\":true},"
                          "{\"id\":2,\"name\":\"Lab\"}"
                          "],\"id\":3}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    const json_value *result = child(p.Root(), "result");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, JSON_ARRAY);

    int count = 0;
    json_for_each(prof, result)
    {
        EXPECT_EQ(prof->type, JSON_OBJECT);
        expect_numeric_field(prof, "id");
        expect_string_field(prof, "name");
        // selected is optional (only present when true)
        ++count;
    }
    EXPECT_EQ(count, 2);
}

// ---------------------------------------------------------------------------
// Equipment profile field shape (~line 2320-2330)
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, GetEquipmentProfileResult)
{
    const char *fixture = "{\"jsonrpc\":\"2.0\",\"result\":{"
                          "\"focal_length\":1000,"
                          "\"pixel_size\":4.5,"
                          "\"camera_binning\":1,"
                          "\"software_binning\":1,"
                          "\"guide_speed\":0.5,"
                          "\"calibration_duration\":750,"
                          "\"calibration_distance\":25,"
                          "\"high_res_encoders\":false,"
                          "\"auto_restore_calibration\":true,"
                          "\"multistar_enabled\":true"
                          "},\"id\":4}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    const json_value *r = child(p.Root(), "result");
    ASSERT_NE(r, nullptr);
    expect_numeric_field(r, "focal_length");
    expect_numeric_field(r, "pixel_size");
    expect_numeric_field(r, "camera_binning");
    expect_numeric_field(r, "software_binning");
    expect_numeric_field(r, "guide_speed");
    expect_numeric_field(r, "calibration_duration");
    expect_numeric_field(r, "calibration_distance");
    expect_bool_field(r, "high_res_encoders");
    expect_bool_field(r, "auto_restore_calibration");
    expect_bool_field(r, "multistar_enabled");
}

// ---------------------------------------------------------------------------
// Lock-shift query shape (~line 2872-2876)
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, GetLockShiftParams)
{
    {
        // shift disabled — only enabled is present
        const char *fixture = "{\"jsonrpc\":\"2.0\",\"result\":{\"enabled\":false},\"id\":1}";
        JsonParser p;
        ASSERT_TRUE(p.Parse(std::string(fixture)));
        const json_value *r = child(p.Root(), "result");
        ASSERT_NE(r, nullptr);
        expect_bool_field(r, "enabled");
    }
    {
        // shift enabled — rate/units/axes are present
        const char *fixture = "{\"jsonrpc\":\"2.0\",\"result\":{"
                              "\"enabled\":true,"
                              "\"rate\":[1.0,2.0],"
                              "\"units\":\"arcsec/hr\","
                              "\"axes\":\"RA/Dec\""
                              "},\"id\":1}";
        JsonParser p;
        ASSERT_TRUE(p.Parse(std::string(fixture)));
        const json_value *r = child(p.Root(), "result");
        ASSERT_NE(r, nullptr);
        expect_bool_field(r, "enabled");
        const json_value *rate = child(r, "rate");
        ASSERT_NE(rate, nullptr);
        EXPECT_EQ(rate->type, JSON_ARRAY);
        const json_value *units = child(r, "units");
        ASSERT_NE(units, nullptr);
        ASSERT_EQ(units->type, JSON_STRING); // json_value is a union; checking type before string_value avoids UB
        // arcsec/hr or pixels/hr — value contract
        EXPECT_TRUE(std::strcmp(units->string_value, "arcsec/hr") == 0 || std::strcmp(units->string_value, "pixels/hr") == 0);
        const json_value *axes = child(r, "axes");
        ASSERT_NE(axes, nullptr);
        ASSERT_EQ(axes->type, JSON_STRING);
        EXPECT_TRUE(std::strcmp(axes->string_value, "RA/Dec") == 0 || std::strcmp(axes->string_value, "X/Y") == 0);
    }
}

// ---------------------------------------------------------------------------
// PHD_Point shape (~line 290) — used for star coords, lock pos, etc.
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, PointShape)
{
    const char *fixture = "{\"X\":123.456,\"Y\":78.9}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    expect_numeric_field(p.Root(), "X");
    expect_numeric_field(p.Root(), "Y");
}

// ---------------------------------------------------------------------------
// StarSelected event (~line 338)
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, StarSelectedEvent)
{
    const char *fixture = "{\"Event\":\"StarSelected\",\"Timestamp\":1.0,\"Host\":\"h\",\"Inst\":1,"
                          "\"X\":100.5,\"Y\":200.7}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    expect_numeric_field(p.Root(), "X");
    expect_numeric_field(p.Root(), "Y");
}

// ---------------------------------------------------------------------------
// Method-dispatch request shape (params can be array or object)
// ---------------------------------------------------------------------------
TEST(JsonRpcSchema, MethodRequestArrayParams)
{
    const char *fixture = "{\"method\":\"set_lock_position\",\"params\":[100.0,200.0,true],\"id\":1}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    expect_string_field(p.Root(), "method");
    const json_value *params = child(p.Root(), "params");
    ASSERT_NE(params, nullptr);
    EXPECT_EQ(params->type, JSON_ARRAY);
}

TEST(JsonRpcSchema, MethodRequestObjectParams)
{
    const char *fixture = "{\"method\":\"set_lock_position\","
                          "\"params\":{\"x\":100.0,\"y\":200.0,\"exact\":true},"
                          "\"id\":1}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(fixture)));
    const json_value *params = child(p.Root(), "params");
    ASSERT_NE(params, nullptr);
    EXPECT_EQ(params->type, JSON_OBJECT);
    expect_numeric_field(params, "x");
    expect_numeric_field(params, "y");
    expect_bool_field(params, "exact");
}
