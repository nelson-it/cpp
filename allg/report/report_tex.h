#ifndef report_tex_mne
#define report_tex_mne

#include <string>
#include <map>

#include <message/message.h>
#include <db/database.h>
#include <db/dbtable.h>
#include <utils/cslist.h>

class ReportTex
{
public:
	typedef std::map<std::string, std::string> Macros;

	std::map<std::string, std::string> userprefs;
private:
	Message msg;

	CsList path;

	std::string dtf;
	std::string df;
    std::string tf;
    std::string intf;

    std::string sdtf;
    std::string stf;
    std::string sintf;

    long landscape;

	enum ERROR_TYPES
	{
		E_OK,
		E_NOREPORT,
		E_NOINIT,
		E_NOHEADER,
		E_NORECORD,
		E_NOTRAILER,
		E_NOEND,
		E_NODATA,
		E_NOREPORTROOT,
		E_WRONG_COLUMN,
		E_MAXERROR
	};

	std::string getFile(std::string file, int is_dir = 0);
public:
	ReportTex();
	~ReportTex();

	int mk_report(Database *db, std::string reportname, int subreport,
			      FILE *out, std::string language, std::string schema, std::string query,
			      CsList *wcol, CsList *wval,
			      CsList *wop, CsList *sort, Macros *macros = NULL, Macros *xml =
				  NULL);

	int getLandscape() { return landscape; }
};

#endif /*report_tex_mne */

