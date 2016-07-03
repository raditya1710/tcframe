#pragma once

#include <cstring>
#include <iostream>
#include <streambuf>
#include <string>
#include <vector>

#include "GenerationResult.hpp"
#include "TestCaseGenerationResult.hpp"
#include "MultipleTestCasesCombinationResult.hpp"
#include "tcframe/failure.hpp"
#include "tcframe/logger.hpp"
#include "tcframe/util.hpp"
#include "tcframe/verifier.hpp"

using std::istreambuf_iterator;
using std::string;
using std::vector;

namespace tcframe {

class GeneratorLogger : public BaseLogger {
public:
    virtual ~GeneratorLogger() {}

    GeneratorLogger(LoggerEngine* engine)
            : BaseLogger(engine)
    {}

    virtual void logIntroduction() {
        engine_->logParagraph(0, "Generating test cases...");
    }

    virtual void logResult(const GenerationResult& result) {
        engine_->logParagraph(0, "");
        if (result.isSuccessful()) {
            engine_->logParagraph(0, "Generation finished. All test cases OK.");
        } else {
            engine_->logParagraph(0, "Generation finished. Some test cases FAILED.");
        }
    }

    virtual void logTestCaseResult(const string& testCaseDescription, const TestCaseGenerationResult& result) {
        if (result.isSuccessful()) {
            engine_->logParagraph(0, "OK");
        } else {
            engine_->logParagraph(0, "FAILED");
            engine_->logParagraph(2, "Description: " + testCaseDescription);
            engine_->logParagraph(2, "Reasons:");

            logFailure(result.failure());
        }
    }

    virtual void logMultipleTestCasesCombinationIntroduction(const string& testCaseBaseId) {
        engine_->logHangingParagraph(1, "Combining test cases into a single file (" + testCaseBaseId + "): ");
    }

    virtual void logMultipleTestCasesCombinationResult(const MultipleTestCasesCombinationResult& result) {
        if (result.isSuccessful()) {
            engine_->logParagraph(0, "OK");
        } else {
            engine_->logParagraph(0, "FAILED");
            engine_->logParagraph(2, "Reasons:");

            logFailure(result.failure());
        }
    }

private:
    void logFailure(Failure* failure) {
        switch (failure->type()) {
            case FailureType::CONSTRAINTS_VERIFICATION:
                logConstraintsVerificationFailure(((ConstraintsVerificationFailure*) failure)->verificationResult());
                break;
            case FailureType::MULTIPLE_TEST_CASES_CONSTRAINTS_VERIFICATION:
                logMultipleTestCasesConstraintsVerificationFailure(
                        ((MultipleTestCasesConstraintsVerificationFailure*) failure)->verificationResult());
                break;
            case FailureType::SOLUTION_EXECUTION:
                logSolutionExecutionFailure(((SolutionExecutionFailure*) failure)->executionResult());
                break;
            default:
                logSimpleFailure(((SimpleFailure*) failure)->message());
        }
    }

    void logConstraintsVerificationFailure(const ConstraintsVerificationResult& result) {
        for (const auto& entry : result.unsatisfiedConstraintDescriptionsBySubtaskId()) {
            int subtaskId = entry.first;
            const vector<string>& unsatisfiedConstraintDescriptions = entry.second;

            if (subtaskId == -1) {
                engine_->logListItem1(2, "Does not satisfy constraints, on:");
            } else {
                engine_->logListItem1(2, "Does not satisfy subtask " + StringUtils::toString(subtaskId) + ", on constraints:");
            }

            for (const string& unsatisfiedConstraintDescription : unsatisfiedConstraintDescriptions) {
                engine_->logListItem2(3, unsatisfiedConstraintDescription);
            }
        }
        for (int subtaskId : result.satisfiedButNotAssignedSubtaskIds()) {
            engine_->logListItem1(2, "Satisfies subtask " + StringUtils::toString(subtaskId) + " but is not assigned to it");
        }
    }

    void logMultipleTestCasesConstraintsVerificationFailure(
            const MultipleTestCasesConstraintsVerificationResult& result) {

        engine_->logListItem1(2, "Does not satisfy constraints, on:");

        for (const string& unsatisfiedConstraintDescription : result.unsatisfiedConstraintDescriptions()) {
            engine_->logListItem2(3, unsatisfiedConstraintDescription);
        }
    }

    void logSolutionExecutionFailure(const ExecutionResult& result) {
        engine_->logListItem1(2, "Execution of solution failed:");
        if (result.exitStatus() <= 128) {
            engine_->logListItem2(3, "Exit code: " + StringUtils::toString(result.exitStatus()));
            engine_->logListItem2(3, "Standard error: " + string(istreambuf_iterator<char>(*result.errorStream()), istreambuf_iterator<char>()));
        } else {
            engine_->logListItem2(3, string(strsignal(result.exitStatus() - 128)));
        }
    }

    void logSimpleFailure(const string& message) {
        engine_->logListItem1(2, message);
    }
 };

}
