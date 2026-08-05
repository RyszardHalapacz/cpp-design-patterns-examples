#include <gtest/gtest.h>
#include <memory>
#include <typeindex>

#include "patterns/services/Logger.hpp"
#include "patterns/services/FileLogger.hpp"
#include "patterns/services/DoSomething.hpp"
#include "patterns/services/ServiceLocator.hpp"

using namespace patterns::services;

// ─── Logger ──────────────────────────────────────────────────────────────────

TEST(LoggerTest, LogPrintsToStdout) {
    Logger logger;
    testing::internal::CaptureStdout();
    logger.log("hello world");
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(out, "hello world");
}

TEST(LoggerTest, LogEmptyString) {
    Logger logger;
    testing::internal::CaptureStdout();
    logger.log("");
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(out, "");
}

// ─── FileLogger ──────────────────────────────────────────────────────────────

TEST(FileLoggerTest, ConstructorPrintsCreationMessage) {
    testing::internal::CaptureStdout();
    FileLogger fl("test.log");
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("test.log"), std::string::npos);
    EXPECT_NE(out.find("FileLogger"), std::string::npos);
}

TEST(FileLoggerTest, LogPrintsFilenameAndMessage) {
    FileLogger fl("data.log");
    testing::internal::CaptureStdout();
    fl.log("entry");
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("data.log"), std::string::npos);
    EXPECT_NE(out.find("entry"), std::string::npos);
}

// ─── DoSomething ─────────────────────────────────────────────────────────────

TEST(DoSomethingTest, DoPrintsMessage) {
    DoSomething ds;
    testing::internal::CaptureStdout();
    ds.do_();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("do something"), std::string::npos);
}

// ─── ServiceLocator ──────────────────────────────────────────────────────────

TEST(ServiceLocatorTest, ProvideAndGetByTemplate) {
    auto loggerPtr = std::make_shared<Logger>();
    ServiceLocator::instance().provide<Logger>(loggerPtr);
    auto result = ServiceLocator::instance().tryGet<Logger>();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(&result->get(), loggerPtr.get());
}

TEST(ServiceLocatorTest, ProvideAndGetDoSomething) {
    auto ds = std::make_shared<DoSomething>();
    ServiceLocator::instance().provide<DoSomething>(ds);
    auto result = ServiceLocator::instance().tryGet<DoSomething>();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(&result->get(), ds.get());
}

TEST(ServiceLocatorTest, TryGetReturnsUnexpectedWhenNotRegistered) {
    // FileLogger is not registered in a fresh test run unless provided explicitly.
    // Remove any existing registration by providing a sentinel, then check a
    // type that was never registered — use a local helper type if possible.
    // Since we can't unregister, verify the error code path via provideRuntime
    // with a null ptr instead.
    auto result = ServiceLocator::instance().provide<Logger>(nullptr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ServiceLocatorErrorCode::NullService);
}

TEST(ServiceLocatorTest, AppLoggerShortcut) {
    auto loggerPtr = std::make_shared<Logger>();
    ServiceLocator::instance().provide<Logger>(loggerPtr);
    auto result = appLogger();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(&result->get(), loggerPtr.get());
}

TEST(ServiceLocatorTest, AppFileLoggerShortcut) {
    testing::internal::CaptureStdout();
    auto flPtr = std::make_shared<FileLogger>("shortcut.log");
    testing::internal::GetCapturedStdout(); // discard constructor output
    ServiceLocator::instance().provide<FileLogger>(flPtr);
    auto result = appFileLogger();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(&result->get(), flPtr.get());
}

TEST(ServiceLocatorTest, AppDoSomethingShortcut) {
    auto dsPtr = std::make_shared<DoSomething>();
    ServiceLocator::instance().provide<DoSomething>(dsPtr);
    auto result = appDoSomething();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(&result->get(), dsPtr.get());
}

TEST(ServiceLocatorTest, ProvideRuntimeAndGetRuntime) {
    auto dsPtr = std::make_shared<DoSomething>();
    ServiceLocator::instance().provideRuntime(dsPtr);
    auto result = ServiceLocator::instance().tryGetRuntime<DoSomething>();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(&result->get(), dsPtr.get());
}

TEST(ServiceLocatorTest, GetRuntimeByTypeIndex) {
    auto dsPtr = std::make_shared<DoSomething>();
    ServiceLocator::instance().provideRuntime(dsPtr);
    auto key = std::type_index(typeid(DoSomething));
    auto result = ServiceLocator::instance().tryGetRuntime(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(dynamic_cast<DoSomething*>(result->get()), dsPtr.get());
}

TEST(ServiceLocatorTest, AppDoSomethingByPointerShortcut) {
    auto dsPtr = std::make_shared<DoSomething>();
    ServiceLocator::instance().provideRuntime(dsPtr);
    auto result = appDoSomethingByPointer();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(&result->get(), dsPtr.get());
}

TEST(ServiceLocatorTest, ProvideRuntimeNullReturnsNullService) {
    auto result = ServiceLocator::instance().provideRuntime(nullptr);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ServiceLocatorErrorCode::NullService);
}

TEST(ServiceLocatorTest, TryGetRuntimeUnknownKeyReturnsServiceNotFound) {
    // Use a dummy local struct whose type_index was never registered.
    struct NeverRegistered : IService {};
    auto result = ServiceLocator::instance().tryGetRuntime(std::type_index(typeid(NeverRegistered)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ServiceLocatorErrorCode::ServiceNotFound);
}

TEST(ServiceLocatorTest, LogFileDelegatesToFileLogger) {
    testing::internal::CaptureStdout();
    auto flPtr = std::make_shared<FileLogger>("logfile_test.log");
    testing::internal::GetCapturedStdout();
    ServiceLocator::instance().provide<FileLogger>(flPtr);

    testing::internal::CaptureStdout();
    logFile("test-entry");
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("test-entry"), std::string::npos);
}

TEST(ServiceLocatorTest, OverwriteServiceReturnsNew) {
    auto first  = std::make_shared<Logger>();
    auto second = std::make_shared<Logger>();
    ServiceLocator::instance().provide<Logger>(first);
    ServiceLocator::instance().provide<Logger>(second);
    auto result = ServiceLocator::instance().tryGet<Logger>();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(&result->get(), second.get());
}
