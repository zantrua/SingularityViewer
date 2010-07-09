//<edit>
#include "llfloater.h"
#include "lltemplatemessagereader.h"
#include "llmessagetamperer.h"

class LLFloaterMessageTamperer : public LLFloater
{
public:
	LLFloaterMessageTamperer();
	~LLFloaterMessageTamperer();

	static LLFloaterMessageTamperer* sInstance;

	static void show();
	BOOL postBuild();
	void refreshTamperedMessages();

	static BOOL onClickTamperIn(void* user_data);
	static BOOL onClickTamperOut(void* user_data);
};
//<edit>
