#include "Utils.h"
#include "catch.hpp"

TEST_CASE("getDirName function")
{
	REQUIRE(getDirName(wxFileName("C:\\A\\test\\path\\with\\a", "file.txt")).GetFullPath() == wxString(wxT("C:\\A\\test\\path\\with\\a\\")));
	REQUIRE(getDirName(wxFileName("/a/test/path", "")) == wxFileName("/a/test/path", ""));
}

TEST_CASE("WithStaticDefault class")
{
	REQUIRE(WithStaticDefault<int, 2000>::DEFAULT_VALUE == 2000);
	REQUIRE(WithStaticDefault<int, 2000>() == 2000);
	REQUIRE(WithStaticDefault<int, 2000>(5) == 5);
}

TEST_CASE("WithStaticDefault class serialization")
{
	REQUIRE(nlohmann::json(WithStaticDefault<int, 2000>(5)) == 5);
	REQUIRE(nlohmann::json(WithStaticDefault<int, 2000>()) == nullptr);
}

TEST_CASE("wxFileName operator/")
{
	REQUIRE(getDirName(wxFileName("C:\\A\\test\\path\\with\\a", "file.txt")) / wxFileName("./second/folder", "file2.txt")
		== wxFileName("C:\\A\\test\\path\\with\\a\\second\\folder", "file2.txt"));
}

TEST_CASE("substituteFormatString function")
{
	REQUIRE(substituteFormatString(wxT("A $1 is $2."), '$', { wxT("storm"), wxT("coming") }, {}) == wxT("A storm is coming."));
	REQUIRE(substituteFormatString(wxT("A $noun is $1."), '$', { wxT("coming") }, { { wxT("noun"), wxT("storm") } }) == wxT("A storm is coming."));
}

TEST_CASE("startsWith function")
{
	REQUIRE(startsWith(std::string("A test string"), std::string("A te")));
	REQUIRE(startsWith(wxFileName("C:\\A\\test\\path", "file.txt").GetDirs(), wxFileName("C:\\A", "").GetDirs()));
	REQUIRE(!startsWith(wxFileName("C:\\A\\test\\path", "file.txt").GetDirs(), wxFileName("C:\\A\\te", "").GetDirs()));
}