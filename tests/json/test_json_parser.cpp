// Tests for src/json_parser.cpp.
//
// json_parser is the deserializer used by every Alpaca response and by the
// event-server's JSON-RPC inbound path. A regression here breaks every
// downstream consumer (NINA, KStars, web UI), so we pin the contract.

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "json_parser.h"

namespace
{

// helper: walk first_child and collect by name
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

}

TEST(JsonParser, RejectsEmptyAndMalformed)
{
    JsonParser p;
    EXPECT_FALSE(p.Parse(std::string("")));
    EXPECT_FALSE(p.Parse(std::string("{")));
    EXPECT_FALSE(p.Parse(std::string("[1, 2,")));
    EXPECT_FALSE(p.Parse(std::string("{a:1}"))) << "unquoted key";
    // Note: vjson is intentionally lenient about some forms (e.g.
    // {"a":}); we don't pin those as either accepted or rejected because
    // both behaviors exist in the wild and we have no callers depending
    // on the result either way.
}

TEST(JsonParser, ParsesPrimitives)
{
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string("{\"i\":42,\"f\":3.5,\"s\":\"hi\",\"b\":true,\"n\":null}")));
    const json_value *root = p.Root();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->type, JSON_OBJECT);

    const json_value *i = child(root, "i");
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->type, JSON_INT);
    EXPECT_EQ(i->int_value, 42);

    const json_value *f = child(root, "f");
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->type, JSON_FLOAT);
    EXPECT_FLOAT_EQ(f->float_value, 3.5f);

    const json_value *s = child(root, "s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->type, JSON_STRING);
    EXPECT_STREQ(s->string_value, "hi");

    const json_value *b = child(root, "b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type, JSON_BOOL);
    EXPECT_EQ(b->int_value, 1);

    const json_value *n = child(root, "n");
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->type, JSON_NULL);
}

TEST(JsonParser, ParsesNegativeAndScientific)
{
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string("{\"a\":-7,\"b\":-1.25,\"c\":1e3,\"d\":-2.5e-2}")));
    const json_value *root = p.Root();

    const json_value *a = child(root, "a");
    EXPECT_EQ(a->type, JSON_INT);
    EXPECT_EQ(a->int_value, -7);

    const json_value *b = child(root, "b");
    EXPECT_EQ(b->type, JSON_FLOAT);
    EXPECT_FLOAT_EQ(b->float_value, -1.25f);

    const json_value *c = child(root, "c");
    EXPECT_EQ(c->type, JSON_FLOAT);
    EXPECT_FLOAT_EQ(c->float_value, 1000.0f);

    const json_value *d = child(root, "d");
    EXPECT_EQ(d->type, JSON_FLOAT);
    EXPECT_FLOAT_EQ(d->float_value, -0.025f);
}

TEST(JsonParser, ParsesArrays)
{
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string("{\"xs\":[1,2,3,4]}")));
    const json_value *xs = child(p.Root(), "xs");
    ASSERT_NE(xs, nullptr);
    EXPECT_EQ(xs->type, JSON_ARRAY);

    int count = 0;
    int sum = 0;
    json_for_each(elt, xs)
    {
        EXPECT_EQ(elt->type, JSON_INT);
        sum += elt->int_value;
        ++count;
    }
    EXPECT_EQ(count, 4);
    EXPECT_EQ(sum, 1 + 2 + 3 + 4);
}

TEST(JsonParser, ParsesNestedObjects)
{
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string("{\"outer\":{\"inner\":{\"deep\":\"v\"}}}")));
    const json_value *outer = child(p.Root(), "outer");
    ASSERT_NE(outer, nullptr);
    const json_value *inner = child(outer, "inner");
    ASSERT_NE(inner, nullptr);
    const json_value *deep = child(inner, "deep");
    ASSERT_NE(deep, nullptr);
    EXPECT_EQ(deep->type, JSON_STRING);
    EXPECT_STREQ(deep->string_value, "v");
}

TEST(JsonParser, ParsesAlpacaResponseShape)
{
    // The shape every Alpaca device returns. If the parser ever loses
    // ability to decode this exact structure, every camera/scope breaks.
    const char *resp = "{\"Value\":42,"
                       "\"ClientTransactionID\":17,"
                       "\"ServerTransactionID\":99,"
                       "\"ErrorNumber\":0,"
                       "\"ErrorMessage\":\"\"}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(resp)));
    const json_value *root = p.Root();

    EXPECT_EQ(child(root, "Value")->int_value, 42);
    EXPECT_EQ(child(root, "ClientTransactionID")->int_value, 17);
    EXPECT_EQ(child(root, "ErrorNumber")->int_value, 0);
    EXPECT_STREQ(child(root, "ErrorMessage")->string_value, "");
}

TEST(JsonParser, ParsesEventServerStartCalibrationRequest)
{
    // Mirrors the JSON-RPC requests that NINA/KStars send. Schema
    // regression here breaks remote control.
    const char *req = "{\"method\":\"start_calibration\","
                      "\"params\":{\"recalibrate\":true},"
                      "\"id\":7}";
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string(req)));
    const json_value *root = p.Root();

    EXPECT_STREQ(child(root, "method")->string_value, "start_calibration");
    EXPECT_EQ(child(root, "id")->int_value, 7);
    const json_value *params = child(root, "params");
    ASSERT_NE(params, nullptr);
    EXPECT_EQ(params->type, JSON_OBJECT);
    EXPECT_EQ(child(params, "recalibrate")->int_value, 1);
}

TEST(JsonParser, MutatesInputBufferOverload)
{
    // The char* Parse() overload is destructive; ensure the std::string
    // overload doesn't leak that mutation back to the caller's value.
    std::string src = "{\"k\":\"v\"}";
    std::string copy = src;
    JsonParser p;
    ASSERT_TRUE(p.Parse(src));
    EXPECT_EQ(src, copy) << "std::string overload must not mutate caller";
}

TEST(JsonParser, EscapesInStrings)
{
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string("{\"s\":\"a\\nb\\tc\\\"d\"}")));
    EXPECT_STREQ(child(p.Root(), "s")->string_value, "a\nb\tc\"d");
}

TEST(JsonParser, ReusableAcrossParses)
{
    JsonParser p;
    ASSERT_TRUE(p.Parse(std::string("{\"a\":1}")));
    EXPECT_EQ(child(p.Root(), "a")->int_value, 1);
    ASSERT_TRUE(p.Parse(std::string("{\"b\":2}")));
    EXPECT_EQ(child(p.Root(), "b")->int_value, 2);
    EXPECT_EQ(child(p.Root(), "a"), nullptr) << "previous parse leaked";
}
