#include "stdafx.h"

#include "CDllExtension.h"

#include "OpenHoldem.h"

#include "CSymbols.h"
#include "CGlobal.h"

#include "grammar.h"
#include "PokerChat.hpp"

CDllExtension		*p_dll_extension = NULL;

CDllExtension::CDllExtension()
{
	__SEH_SET_EXCEPTION_HANDLER

	__SEH_HEADER

	_hmod_dll = NULL;

	__SEH_LOGFATAL("CDll::Constructor :\n");
}

CDllExtension::~CDllExtension()
{
}

void CDllExtension::PassStateToDll(const SHoldemState *pstate)
{
	__SEH_HEADER

	if (_hmod_dll==NULL)
		return;

	(_process_message) ("state", pstate);

	__SEH_LOGFATAL("CDll::PassStateToDll :\n");
}

void CDllExtension::LoadDll(const char * path)
{
	__SEH_HEADER

	CString		t = "", formula_dll = "";
	int			N = 0, i = 0;
	DWORD		err1 = 0, err2 = 0;

	if (_hmod_dll!=NULL)
		return;

	// try to load specific patch if passed in as a parameter
	if (strlen(path))
	{
		_hmod_dll = LoadLibrary(path);
		err1 = GetLastError();

		// If DLL is not loaded, pop an error
		if (_hmod_dll==NULL)
		{
			t.Format("Unable to load DLL from:%s, error=%d\n", path, err1);
			MessageBox(NULL, t, "DLL Load Error", MB_OK | MB_TOPMOST);
			return;
		}
	}

	else
	{

		// Find DLL name to load from the formula file
		N = p_global->formula.mFunction.GetSize();
		formula_dll = "";
		for (i=0; i<N; i++)
		{
			if (p_global->formula.mFunction[i].func == "dll")
			{
				formula_dll = p_global->formula.mFunction[i].func_text;
				i = N + 1;
			}
		}
		formula_dll.Trim();

		// Try to load dll from the ##dll## section, if it is specified
		if (formula_dll != "")
		{
			t.Format("%s\\%s", _startup_path, formula_dll.GetString());
			SetCurrentDirectory(_startup_path);
			_hmod_dll = LoadLibrary(t.GetString());
			err1 = GetLastError();
		}

		// If dll is still not loaded, load from name in Edit/Preferences
		if (_hmod_dll==NULL)
		{
			t.Format("%s\\%s", _startup_path, p_global->preferences.dll_name.GetString());
			SetCurrentDirectory(_startup_path);
			_hmod_dll = LoadLibrary(t.GetString());
			err2 = GetLastError();
		}

		// If it is still not loaded, pop an error
		if (_hmod_dll==NULL)
		{
			t.Format("Unable to load DLL from:\n%s, error=%d\n-or-\n%s, error=%d",
					 p_global->preferences.dll_name.GetString(), err1,
					 formula_dll.GetString(), err2);
			MessageBox(NULL, t, "DLL Load Error", MB_OK | MB_TOPMOST);
			return;
		}
	}

	if (_hmod_dll)
	{
		// Get address of process_message from dll
		// whuser.dll does not ship with a .def file by default - we must use the ordinal method to get the address
		//global.process_message = (process_message_t) GetProcAddress(global._hmod_dll, "process_message");
		_process_message = (process_message_t) ::GetProcAddress(_hmod_dll, (LPCSTR) 1);

		if (_process_message==NULL)
		{
			t.Format("Unable to find process_message in dll");
			MessageBox(NULL, t, "DLL Load Error", MB_OK | MB_TOPMOST);
			FreeLibrary(_hmod_dll);
			_hmod_dll = NULL;
		}
		else
		{
			// pass "load" message
			(_process_message) ("event", "load");

			// pass "pfgws" message
			(_process_message) ("pfgws", GetSymbolFromDll);

			//pass "phl1k" message (address of handlist arrays)
			//  2008-03-22 Matrix
			(_process_message) ("phl1k", p_global->formula.inlist);

			//pass "prw1326" message (address of prw1326 structure)
			//  2008-05-08 Matrix
			(_process_message) ("prw1326", p_symbols->prw1326());

			//  2008.02.27 by THF
			//
			//  pass "p_send_chatMessage" message
			//
			//  Providing a pointer to the chat function,
			//	which can be used inside the dll,
			//	similar to "pfgws".
			//
			(_process_message)(Pointer_for__send_ChatMessage,
							  get_Pointer_to__send_ChatMessage());
		}
	}

	__SEH_LOGFATAL("CDll::LoadDll :\n");
}

void CDllExtension::UnloadDll(void)
{
	__SEH_HEADER

	if (_hmod_dll==NULL)
		return;

	EnterCriticalSection(&p_symbols->cs_symbols);
	p_symbols->set_prw1326()->useme=0;
	LeaveCriticalSection(&p_symbols->cs_symbols);

	(_process_message) ("event", "unload");

	if (FreeLibrary(_hmod_dll))
		_hmod_dll = NULL;

	__SEH_LOGFATAL("CDll::UnloadDll :\n");
}

const bool CDllExtension::IsDllLoaded()
{
	__SEH_HEADER

	return _hmod_dll != NULL;

	__SEH_LOGFATAL("CDll::IsDllLoaded :\n");
}

double GetSymbolFromDll(const int chair, const char* name, bool& iserr)
{
	__SEH_HEADER

	int			e = SUCCESS, stopchar = 0;
	double		res = 0.;
	CString		str = "";
	boost::spirit::tree_parse_info<const char *, int_factory_t>	tpi;
	bool		result = false;

	str.Format("%s", name);

	EnterCriticalSection(&cs_parse);
	result = parse(&str, &tpi, &stopchar);
	LeaveCriticalSection(&cs_parse);
	if (result)
	{
		e = SUCCESS;
		res = evaluate(&p_global->formula, tpi, NULL, &e);
	}
	else
	{
		res = 0;
		e = ERR_INVALID_FUNC_SYM;
	}

	if (e == SUCCESS)
		iserr = false;

	else
		iserr = true;

	return res;

	__SEH_LOGFATAL("::GetSymbolFromDll :\n");
}
