#ifndef TPLCC_TESTS_MOCKING_REPORT_ERROR_STUB_H
#define TPLCC_TESTS_MOCKING_REPORT_ERROR_STUB_H

#include "tplcc/error.h"

struct ReportErrorStub : IReportError {
	std::vector<Error> listOfErrors{};

	void reportsError(Error error) override {
		listOfErrors.push_back(std::move(error));
	}
};

#endif