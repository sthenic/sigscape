#include "data_types.h"
#include "CppUTest/TestHarness.h"

#include <numeric>
#include <cmath>

TEST_GROUP(Value)
{
    std::vector<std::vector<double>> RECORDS{};

    void setup() override
    {
        RECORDS.push_back({4, 0, 1, 2, 3, 4, -10, 5, 17, 6, 7, 8, 9});
        RECORDS.push_back({5, 6, 73, -80, 54, 2, 44});
        RECORDS.push_back({8, 5, 66, 34, -54, -9, 12, 0, 89, -33});
        RECORDS.push_back({6, 7, 8, -9, -11, 42, 8});
    }
};

TEST(Value, Max)
{
    ValueWithStatistics max{Value::Properties("V", "8.3")};

    for (const auto &record : RECORDS)
        max = *std::max_element(record.cbegin(), record.cend());

    /* Always reflects the last record analyzed. */
    DOUBLES_EQUAL(42, max.value, 0.01);

    DOUBLES_EQUAL(17, max.Min().value, 0.01);
    DOUBLES_EQUAL(89, max.Max().value, 0.01);
    DOUBLES_EQUAL(55.25, max.Mean().value, 0.01);
}

TEST(Value, Min)
{
    ValueWithStatistics min{Value::Properties("V", "8.3")};

    for (const auto &record : RECORDS)
        min = *std::min_element(record.cbegin(), record.cend());

    DOUBLES_EQUAL(-11, min.value, 0.01);
    DOUBLES_EQUAL(-80, min.Min().value, 0.01);
    DOUBLES_EQUAL(-10, min.Max().value, 0.01);
    DOUBLES_EQUAL(-38.75, min.Mean().value, 0.01);
}

TEST(Value, Mean)
{
    ValueWithStatistics mean{Value::Properties("V", "8.3")};

    for (const auto &record : RECORDS)
    {
        const auto sum = std::accumulate(record.cbegin(), record.cend(), 0.0);
        mean = sum / static_cast<double>(record.size());
    }

    DOUBLES_EQUAL(7.28, mean.value, 0.01);
    DOUBLES_EQUAL(4.31, mean.Min().value, 0.01);
    DOUBLES_EQUAL(14.86, mean.Max().value, 0.01);
    DOUBLES_EQUAL(9.56, mean.Mean().value, 0.01);
}

TEST(Value, StandardDeviation)
{
    ValueWithStatistics sdev{Value::Properties("V", "8.3")};

    for (const auto &record : RECORDS)
    {
        const auto sum = std::accumulate(record.cbegin(), record.cend(), 0.0);
        const auto mean = sum / static_cast<double>(record.size());

        double tmp{0};
        std::for_each(record.cbegin(), record.cend(), [&](const double &d){
            double diff = d - mean;
            tmp += diff * diff;
        });

        tmp /= static_cast<double>(record.size());
        tmp = std::sqrt(tmp);

        sdev = tmp;
    }

    DOUBLES_EQUAL(16.06, sdev.value, 0.01);
    DOUBLES_EQUAL(5.88, sdev.Min().value, 0.01);
    DOUBLES_EQUAL(46.45, sdev.Max().value, 0.01);
    DOUBLES_EQUAL(27.21, sdev.Mean().value, 0.01);
}
