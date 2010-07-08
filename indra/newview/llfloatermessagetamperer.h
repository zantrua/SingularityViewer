//<edit>
#include "llfloater.h"
#include "lltemplatemessagereader.h"

class LLFloaterMessageTamperer : public LLFloater
{
public:
	LLFloaterMessageTamperer();
	~LLFloaterMessageTamperer();

	static LLFloaterMessageTamperer* sInstance;

	static std::map<std::string, int> tamperedTypes;
	static bool tamperingAny;

	static void show();
	BOOL postBuild();
	void refreshTamperedMessages();
	static bool isTampered(std::string messageType, bool inbound);

	static BOOL onClickTamperIn(void* user_data);
	static BOOL onClickTamperOut(void* user_data);

	enum messageDirection
	{
		NONE,
		INBOUND,
		OUTBOUND
	};
};
//<edit>
