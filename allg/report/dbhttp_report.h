#ifndef dbhttp_report_mne
#define dbhttp_report_mne

#include <map>
#include <string>

#include <message/message.h>
#include <embedweb/http_translate.h>
#include <db/dbhttp_provider.h>

class DbHttpReport : public DbHttpProvider
{

	enum ErrorType
	{
		E_PDF_OPEN = 1,
		E_PDF_WHERE,
		E_PDF_WVAL,
		E_PDF_READ,
		E_AUTO_TMPFILE,
		E_AUTO_STATUS,
		MAX_ERROR = 1000
	};

	enum WarningType
	{
		W_REPORTRUNNING = 1,
		W_NOREPORT,
		W_NOROWS,
		MAX_WARNING = 1000
	};

    typedef void ( DbHttpReport::*SubProvider)(Database *db, HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;

    SubProviderMap subprovider;

	void start_function(Database *db, DbQuery *query, DbConnect::ResultVec *rv, std::string schema, std::string function, CsList cols );
	DbTable::ValueMap mk_pdfwhere(HttpHeader *h, CsList *pdfwcol, CsList *pdfwval);

protected:

    Message msg;
    void mk_auto (Database *db, HttpHeader *h);
    void index (Database *db, HttpHeader *h, const char *report);

    void header_html( Database *db, HttpHeader *h);

public:
    DbHttpReport( DbHttp *h );
    virtual ~DbHttpReport();

    virtual std::string getPath() { return "report"; }
    virtual int request (Database *db, HttpHeader *h);

};

#endif /* dbhttp_report_mne */
