#include <gtest/gtest.h>
#include "../../examples/volumetric_integration/VolumetricOptions.h"
#include <initializer_list>

namespace {
auto parse(std::initializer_list<std::string_view> args)
{
    return ku::volumetric::parseOptions(std::span(args.begin(),args.size()));
}
TEST(VolumetricOptions, DefaultsMatchReferenceFreeMode)
{
    const auto s = parse({}).settings;
    EXPECT_EQ(s.steps,100); EXPECT_EQ(s.shadowSteps,16); EXPECT_EQ(s.integration,0);
    EXPECT_FLOAT_EQ(s.density,1); EXPECT_FLOAT_EQ(s.light,1); EXPECT_FLOAT_EQ(s.strongFog,0);
    EXPECT_TRUE(s.noise); EXPECT_TRUE(s.shadows); EXPECT_FALSE(s.paused);
}
TEST(VolumetricOptions, FixedSubmissionTimeAndRuntimeArguments)
{
    const auto result = parse({"--smoke-frames","4","--volumetric-time","1.25","--volumetric-time-step","0.5","--frames-in-flight","2"});
    EXPECT_EQ(result.forwarded,(std::vector<std::string>{"--smoke-frames","4","--frames-in-flight","2"}));
    EXPECT_TRUE(result.settings.paused); EXPECT_FLOAT_EQ(result.settings.time,1.25f); EXPECT_FLOAT_EQ(result.settings.timeStep,.5f);
}
TEST(VolumetricOptions, SupportedModesAndExtremes)
{
    EXPECT_EQ(parse({"--volumetric-integration","split"}).settings.integration,3);
    EXPECT_EQ(parse({"--volumetric-integration","transmittance-first"}).settings.integration,2);
    const auto s = parse({"--volumetric-density","0","--volumetric-light","0","--volumetric-steps","16","--volumetric-shadow-steps","64","--volumetric-no-noise","--volumetric-no-shadows"}).settings;
    EXPECT_FLOAT_EQ(s.density,0); EXPECT_FLOAT_EQ(s.light,0); EXPECT_EQ(s.steps,16);
    EXPECT_FALSE(s.noise); EXPECT_FALSE(s.shadows);
}
TEST(VolumetricOptions, RejectsMissingNonfiniteTrailingAndOutOfRangeValues)
{
    for (const auto value : {"nan","inf","-1","20.1","1suffix",""})
        EXPECT_THROW(parse({"--volumetric-density",value}),std::invalid_argument);
    for (const auto value : {"0","15","257","100.5","32suffix"})
        EXPECT_THROW(parse({"--volumetric-steps",value}),std::invalid_argument);
    EXPECT_THROW(parse({"--volumetric-time"}),std::invalid_argument);
    EXPECT_THROW(parse({"--volumetric-time-step","1"}),std::invalid_argument);
    EXPECT_THROW(parse({"--volumetric-integration","other"}),std::invalid_argument);
    EXPECT_THROW(parse({"--volumetric-readback-prefix",""}),std::invalid_argument);
    EXPECT_THROW(parse({"--volumetric-unknown"}),std::invalid_argument);
}
}
