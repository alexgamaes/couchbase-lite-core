//
//  c4QueryTest.cc
//  LiteCore
//
//  Created by Jens Alfke on 12/16/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "c4Test.hh"
#include "c4DBQuery.h"
#include <iostream>

using namespace std;


class QueryTest : public C4Test {
public:
    QueryTest() :C4Test(1) {  // always use SQLite + version vectors
        importJSONLines(sFixturesDir + "names_100.json");
    }

    ~QueryTest() {
        c4query_free(query);
    }

    C4Query* compile(const char *expr, const char *sort =nullptr) {
        C4Error error;
        c4query_free(query);
        query = c4query_new(db, c4str(expr), c4str(sort), &error);
        char errbuf[256];
        INFO("error " << error.domain << "/" << error.code << ": " << c4error_getMessageC(error, errbuf, sizeof(errbuf)));
        REQUIRE(query);
        return query;
    }

    C4Query* compile(const std::string &expr, const std::string &sort ="") {
        return compile(expr.c_str(), (sort.empty() ? nullptr : sort.c_str()));
    }

    std::vector<std::string> run(uint64_t skip =0, uint64_t limit =UINT64_MAX,
                                 const char *bindings =nullptr)
    {
        REQUIRE(query);
        std::vector<std::string> docIDs;
        C4QueryOptions options = kC4DefaultQueryOptions;
        options.skip = skip;
        options.limit = limit;
        C4Error error;
        auto e = c4query_run(query, &options, c4str(bindings), &error);
        INFO("c4query_run got error " << error.domain << "/" << error.code);
        REQUIRE(e);
        while (c4queryenum_next(e, &error)) {
            std::string docID((const char*)e->docID.buf, e->docID.size);
            docIDs.push_back(docID);
        }
        CHECK(error.code == 0);
        c4queryenum_free(e);
        return docIDs;
    }

private:
    C4Query *query {nullptr};
};


TEST_CASE_METHOD(QueryTest, "DB Query", "[Query][C]") {
    compile(json5("['=', ['.', 'contact', 'address', 'state'], 'CA']"));
    CHECK(run() == (vector<string>{"0000001", "0000015", "0000036", "0000043", "0000053", "0000064", "0000072", "0000073"}));
    CHECK(run(1, 8) == (vector<string>{"0000015", "0000036", "0000043", "0000053", "0000064", "0000072", "0000073"}));
    CHECK(run(1, 4) == (vector<string>{"0000015", "0000036", "0000043", "0000053"}));

#if 0 //TEMP: Not currently supported until I add array operators
    compile("{\"contact.phone\": {\"$elemMatch\": {\"$like\": \"%97%\"}}}");
    CHECK(run() == (vector<string>{"0000013", "0000014", "0000027", "0000029", "0000045", "0000048", "0000070", "0000085", "0000096"}));
#endif

    compile(json5("['AND', ['=', ['count()', ['.', 'contact', 'phone']], 2],\
                           ['=', ['.', 'gender'], 'male']]"));
    CHECK(run() == (vector<string>{"0000002", "0000014", "0000017", "0000027", "0000031", "0000033", "0000038", "0000039", "0000045", "0000047",
        "0000049", "0000056", "0000063", "0000065", "0000075", "0000082", "0000089", "0000094", "0000097"}));
}

TEST_CASE_METHOD(QueryTest, "DB Query sorted", "[Query][C]") {
    compile(json5("['=', ['.', 'contact', 'address', 'state'], 'CA']"),
            "[\"name.last\"]");
    CHECK(run() == (vector<string>{"0000015", "0000036", "0000072", "0000043", "0000001", "0000064", "0000073", "0000053"}));
}


TEST_CASE_METHOD(QueryTest, "DB Query bindings", "[Query][C]") {
    compile(json5("['=', ['.', 'contact', 'address', 'state'], ['$', 1]]"));
    CHECK(run(0, UINT64_MAX, "{\"1\": \"CA\"}") == (vector<string>{"0000001", "0000015", "0000036", "0000043", "0000053", "0000064", "0000072", "0000073"}));
    compile(json5("['=', ['.', 'contact', 'address', 'state'], ['$', 'state']]"));
    CHECK(run(0, UINT64_MAX, "{\"state\": \"CA\"}") == (vector<string>{"0000001", "0000015", "0000036", "0000043", "0000053", "0000064", "0000072", "0000073"}));
}


 TEST_CASE_METHOD(QueryTest, "Full-text query", "[Query][C]") {
    C4Error err;
    REQUIRE(c4db_createIndex(db, C4STR("contact.address.street"), kC4FullTextIndex, nullptr, &err));
    compile(json5("['MATCH', ['.', 'contact', 'address', 'street'], 'Hwy']"));
    CHECK(run(0, UINT64_MAX) == (vector<string>{"0000013", "0000015", "0000043", "0000044", "0000052"}));

}
