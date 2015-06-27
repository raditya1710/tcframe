#ifndef TCFRAME_SUBMITTER_H
#define TCFRAME_SUBMITTER_H

#include "generator.hpp"
#include "logger.hpp"
#include "os.hpp"
#include "problem.hpp"
#include "util.hpp"
#include "verdict.hpp"

#include <algorithm>
#include <csignal>
#include <map>
#include <set>
#include <string>
#include <vector>

using std::map;
using std::max;
using std::set;
using std::string;
using std::vector;
using tcframe::BaseGenerator;
using tcframe::SubmitterLogger;
using tcframe::OperatingSystem;
using tcframe::DefaultSubmitterLogger;
using tcframe::UnixOperatingSystem;
using tcframe::Util;
using tcframe::Verdict;

namespace tcframe {

template<typename TProblem>
class Submitter {
public:
    Submitter(BaseGenerator<TProblem>* generator)
        : logger(new DefaultSubmitterLogger()),
          os(new UnixOperatingSystem()),
          generator(generator) { }

    int submit(string submissionCommand) {
        if (!isPorcelain) {
            logger->logIntroduction();
        }

        map<int, Verdict> subtaskVerdicts;
        for (Subtask* subtask : generator->getSubtasks()) {
            subtaskVerdicts[subtask->getId()] = Verdict::accepted();
        }

        for (TestGroup* testGroup : generator->getTestData()) {
            int testGroupId = testGroup->getId();

            if (!isPorcelain) {
                logger->logTestGroupIntroduction(testGroupId);
            }

            for (int testCaseId = 1; testCaseId <= testGroup->getTestCasesCount(); testCaseId++) {
                TestCase* testCase = testGroup->getTestCase(testCaseId - 1);
                string testCaseName = Util::constructTestCaseName(generator->getSlug(), testGroup->getId(), testCaseId);
                Verdict verdict = submitOnTestCase(testCaseName, submissionCommand);
                for (int subtaskId : testCase->getSubtaskIds()) {
                    subtaskVerdicts[subtaskId] = max(subtaskVerdicts[subtaskId], verdict);
                }
            }
        }

        if (!isPorcelain) {
            logger->logSubmissionResult(subtaskVerdicts);
        } else {
            logger->logPorcelainSubmissionResult(subtaskVerdicts);
        }

        return 0;
    }

    void setPorcelain(bool isPorcelain) {
        this->isPorcelain = isPorcelain;
    }

private:
    SubmitterLogger* logger;
    OperatingSystem* os;
    BaseGenerator<TProblem>* generator;

    bool isPorcelain;

    Verdict submitOnTestCase(string testCaseName, string submissionCommand) {

        if (!isPorcelain) {
            logger->logTestCaseIntroduction(testCaseName);
        }

        Verdict verdict = gradeOnTestCase(testCaseName, submissionCommand);
        os->removeFile("_submission.out");
        os->removeFile("_diff.out");

        if (!isPorcelain) {
            logger->logTestCaseVerdict(verdict);
        }

        if (!verdict.isAccepted()) {
            if (!isPorcelain) {
                logger->logFailures(verdict.getFailures());
            }
        }

        return verdict;
    }

    Verdict gradeOnTestCase(string testCaseName, string submissionCommand) {
        Verdict verdict = executeOnTestCase(testCaseName, submissionCommand);
        if (verdict.isUnknown()) {
            return scoreOnTestCase(testCaseName);
        }
        return verdict;
    }

    Verdict executeOnTestCase(string testCaseName, string submissionCommand) {
        string testCaseInputFilename = generator->getTestCasesDir() + "/" + testCaseName + ".in";

        os->limitExecutionTime(generator->getTimeLimit());
        os->limitExecutionMemory(generator->getMemoryLimit());
        ExecutionResult result = os->execute(testCaseName + "-submission-evaluation", submissionCommand, testCaseInputFilename, "_submission.out", "_error.out");
        os->limitExecutionTime(0);
        os->limitExecutionMemory(0);

        if (result.exitStatus == 0) {
            return Verdict::unknown();
        }

        vector<Failure> failures;

        if (result.exitStatus & (1<<7)) {
            int signal = WTERMSIG(result.exitStatus);

            if (signal == SIGXCPU) {
                return Verdict::timeLimitExceeded();
            }

            failures.push_back(Failure("Execution of submission failed:", 0));
            failures.push_back(Failure(string(strsignal(signal)), 1));
        } else {
            failures.push_back(Failure("Execution of submission failed:", 0));
            failures.push_back(Failure("Exit code: " + Util::toString(result.exitStatus), 1));
            failures.push_back(Failure("Standard error: " + string(istreambuf_iterator<char>(*result.errorStream), istreambuf_iterator<char>()), 1));
        }
        return Verdict::runtimeError(failures);
    }

    Verdict scoreOnTestCase(string testCaseName) {
        string testCaseOutputFilename = generator->getTestCasesDir() + "/" + testCaseName + ".out";

        string diffCommand = "diff --unchanged-line-format=' %.2dn    %L' --old-line-format='(expected) [line %.2dn]    %L' --new-line-format='(received) [line %.2dn]    %L' " + testCaseOutputFilename + " _submission.out | head -n 10";
        ExecutionResult result = os->execute(testCaseName + "-submission-scoring", diffCommand, "", "_diff.out", "");

        string briefDiffCommand = "diff --brief _submission.out " + testCaseOutputFilename;
        ExecutionResult briefResult = os->execute(testCaseName + "-submission-scoring-brief", briefDiffCommand, "", "", "");

        if (briefResult.exitStatus == 0) {
            return Verdict::accepted();
        } else {
            string diff = string(istreambuf_iterator<char>(*result.outputStream), istreambuf_iterator<char>());
            return Verdict::wrongAnswer({
                    Failure("Diff:\n" + diff, 0)
            });
        }
    }
};

}

#endif
