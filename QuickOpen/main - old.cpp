#include <cppcms/application.h>
#include <cppcms/applications_pool.h>  
#include <cppcms/service.h>  
#include <cppcms/http_response.h>
#include <cppcms/url_mapper.h>
#include <cppcms/url_dispatcher.h>

#include <filesystem>

const std::filesystem::path STATIC_PATH = std::filesystem::current_path() / "static";

template<typename T>
bool startsWith(const T& haystack, const T& needle)
{
	auto haystackIter = haystack.begin(), needleIter = needle.begin();
	
	for (; haystackIter != haystack.end() && needleIter != needle.end();
			++haystackIter, ++needleIter)
	{
		if (*haystackIter != *needleIter) return false;
	}

	return (needleIter == needle.end());
}

class QuickOpenApplication : public cppcms::application
{
	void serveStaticFile(std::string path)
	{
		std::filesystem::path resolvedPath = (STATIC_PATH / path).lexically_normal();

		if (!startsWith(resolvedPath, STATIC_PATH))
		{
			this->response().status(403);
		}
		else
		{
			this->response().out() << "Static content. Path: " << resolvedPath;
		}
	}
public:
	QuickOpenApplication(cppcms::service& svc) : application(svc)
	{
		dispatcher().assign("/static/(.*)", &QuickOpenApplication::serveStaticFile, this, 1);
		mapper().assign("static", "/static/{1}");
	}

	//virtual void main(std::string url) override
	//{
	//	this->response().out() <<
	//		"<html>\n"
	//		"	<body>\n"
	//		"		<p>Hello, world!</p>\n"
	//		"	</body>\n"
	//		"</html>";
	//}
};

int main(int argc, char** argv)
{
	cppcms::service appSvc(argc, argv);
	appSvc.applications_pool().mount(cppcms::applications_factory<QuickOpenApplication>());
	appSvc.run();
}