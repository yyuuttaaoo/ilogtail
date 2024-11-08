// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include "common/JsonUtil.h"
#include "models/MetricEvent.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class MetricEventUnittest : public ::testing::Test {
public:
    void TestName();
    void TestValue();
    void TestTag();
    void TestSize();
    void TestReset();
    void TestToJson();
    void TestFromJson();
    void TestTagsIterator();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
        mMetricEvent = mEventGroup->CreateMetricEvent();
    }

private:
    shared_ptr<SourceBuffer> mSourceBuffer;
    unique_ptr<PipelineEventGroup> mEventGroup;
    unique_ptr<MetricEvent> mMetricEvent;
};

void MetricEventUnittest::TestName() {
    mMetricEvent->SetName("test");
    APSARA_TEST_EQUAL("test", mMetricEvent->GetName().to_string());
}

void MetricEventUnittest::TestValue() {
    mMetricEvent->SetValue(UntypedSingleValue{10.0});
    APSARA_TEST_TRUE(mMetricEvent->Is<UntypedSingleValue>());
    APSARA_TEST_EQUAL(10.0, mMetricEvent->GetValue<UntypedSingleValue>()->mValue);

    mMetricEvent->SetValue<UntypedSingleValue>(100.0);
    APSARA_TEST_TRUE(mMetricEvent->Is<UntypedSingleValue>());
    APSARA_TEST_EQUAL(100.0, mMetricEvent->GetValue<UntypedSingleValue>()->mValue);
}

void MetricEventUnittest::TestTag() {
    {
        string key = "key1";
        string value = "value1";
        mMetricEvent->SetTag(key, value);
        APSARA_TEST_TRUE(mMetricEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mMetricEvent->GetTag(key).to_string());
    }
    {
        string key = "key2";
        string value = "value2";
        mMetricEvent->SetTag(StringView(key.data(), key.size()), StringView(value.data(), value.size()));
        APSARA_TEST_TRUE(mMetricEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mMetricEvent->GetTag(key).to_string());
    }
    {
        string key = "key3";
        string value = "value3";
        mMetricEvent->SetTagNoCopy(mMetricEvent->GetSourceBuffer()->CopyString(key),
                                   mMetricEvent->GetSourceBuffer()->CopyString(value));
        APSARA_TEST_TRUE(mMetricEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mMetricEvent->GetTag(key).to_string());
    }
    {
        string key = "key4";
        string value = "value4";
        const StringBuffer& kb = mMetricEvent->GetSourceBuffer()->CopyString(key);
        const StringBuffer& vb = mMetricEvent->GetSourceBuffer()->CopyString(value);
        mMetricEvent->SetTagNoCopy(StringView(kb.data, kb.size), StringView(vb.data, vb.size));
        APSARA_TEST_TRUE(mMetricEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mMetricEvent->GetTag(key).to_string());
    }
    {
        string key = "key5";
        APSARA_TEST_FALSE(mMetricEvent->HasTag(key));
        APSARA_TEST_EQUAL("", mMetricEvent->GetTag(key).to_string());
    }
    {
        string key = "key1";
        mMetricEvent->DelTag(key);
        APSARA_TEST_FALSE(mMetricEvent->HasTag(key));
        APSARA_TEST_EQUAL("", mMetricEvent->GetTag(key).to_string());
    }
}

void MetricEventUnittest::TestSize() {
    size_t basicSize = sizeof(time_t) + sizeof(long) + sizeof(UntypedSingleValue) + sizeof(map<StringView, StringView>);
    mMetricEvent->SetName("test");
    basicSize += 4;

    mMetricEvent->SetValue(UntypedSingleValue{10.0});

    // add tag, and key not existed
    mMetricEvent->SetTag(string("key1"), string("a"));
    APSARA_TEST_EQUAL(basicSize + 5U, mMetricEvent->DataSize());

    // add tag, and key existed
    mMetricEvent->SetTag(string("key1"), string("bb"));
    APSARA_TEST_EQUAL(basicSize + 6U, mMetricEvent->DataSize());

    // delete tag
    mMetricEvent->DelTag(string("key1"));
    APSARA_TEST_EQUAL(basicSize, mMetricEvent->DataSize());
}

void MetricEventUnittest::TestReset() {
    mMetricEvent->SetTimestamp(12345678901);
    mMetricEvent->SetName("test");
    mMetricEvent->SetValue(UntypedSingleValue{10.0});
    mMetricEvent->SetTag(string("key1"), string("value1"));
    mMetricEvent->Reset();
    APSARA_TEST_EQUAL(0, mMetricEvent->GetTimestamp());
    APSARA_TEST_FALSE(mMetricEvent->GetTimestampNanosecond().has_value());
    APSARA_TEST_EQUAL("", mMetricEvent->GetName());
    APSARA_TEST_TRUE(mMetricEvent->Is<monostate>());
    APSARA_TEST_EQUAL(mMetricEvent->TagsEnd(), mMetricEvent->TagsBegin());
}

void MetricEventUnittest::TestToJson() {
    mMetricEvent->SetTimestamp(12345678901, 0);
    mMetricEvent->SetName("test");
    mMetricEvent->SetValue(UntypedSingleValue{10.0});
    mMetricEvent->SetTag(string("key1"), string("value1"));
    Json::Value res = mMetricEvent->ToJson();

    Json::Value eventJson;
    string eventStr = R"({
        "name": "test",
        "tags": {
            "key1": "value1"
        },
        "timestamp" : 12345678901,
        "timestampNanosecond" : 0,
        "type" : 2,
        "value": {
            "type": "untyped_single_value",
            "detail": 10.0
        }
    })";
    string errorMsg;
    ParseJsonTable(eventStr, eventJson, errorMsg);

    APSARA_TEST_TRUE(eventJson == res);
}

void MetricEventUnittest::TestFromJson() {
    Json::Value eventJson;
    string eventStr = R"({
        "name": "test",
        "tags": {
            "key1": "value1"
        },
        "timestamp" : 12345678901,
        "timestampNanosecond" : 0,
        "value": {
            "type": "untyped_single_value",
            "detail": 10.0
        }
    })";
    string errorMsg;
    ParseJsonTable(eventStr, eventJson, errorMsg);
    mMetricEvent->FromJson(eventJson);

    APSARA_TEST_EQUAL(12345678901, mMetricEvent->GetTimestamp());
    APSARA_TEST_EQUAL(0L, mMetricEvent->GetTimestampNanosecond().value());
    APSARA_TEST_EQUAL("test", mMetricEvent->GetName());
    APSARA_TEST_TRUE(mMetricEvent->Is<UntypedSingleValue>());
    APSARA_TEST_EQUAL(10.0, mMetricEvent->GetValue<UntypedSingleValue>()->mValue);
    APSARA_TEST_EQUAL("value1", mMetricEvent->GetTag("key1").to_string());
}

void MetricEventUnittest::TestTagsIterator() {
    string key1 = "key1";
    string value1 = "value1";
    string key2 = "key2";
    string value2 = "value2";
    string key3 = "key3";
    string value3 = "value3";

    map<string, string> rawMap;
    rawMap[key1] = value1;
    rawMap[key2] = value2;
    rawMap[key3] = value3;

    mMetricEvent->SetTag(key1, value1);
    mMetricEvent->SetTag(key2, value2);
    mMetricEvent->SetTag(key3, value3);

    for (auto it = mMetricEvent->TagsBegin(); it != mMetricEvent->TagsEnd(); ++it) {
        APSARA_TEST_EQUAL(rawMap[it->first.to_string()], it->second.to_string());
    }
    APSARA_TEST_EQUAL((size_t)3, mMetricEvent->TagsSize());
}

UNIT_TEST_CASE(MetricEventUnittest, TestName)
UNIT_TEST_CASE(MetricEventUnittest, TestValue)
UNIT_TEST_CASE(MetricEventUnittest, TestTag)
UNIT_TEST_CASE(MetricEventUnittest, TestSize)
UNIT_TEST_CASE(MetricEventUnittest, TestReset)
UNIT_TEST_CASE(MetricEventUnittest, TestToJson)
UNIT_TEST_CASE(MetricEventUnittest, TestFromJson)
UNIT_TEST_CASE(MetricEventUnittest, TestTagsIterator)

} // namespace logtail

UNIT_TEST_MAIN
