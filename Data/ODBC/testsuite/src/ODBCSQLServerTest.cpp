//
// ODBCSQLServerTest.cpp
//
// $Id: //poco/1.4/Data/ODBC/testsuite/src/ODBCSQLServerTest.cpp#1 $
//
// Copyright (c) 2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#include "ODBCSQLServerTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/String.h"
#include "Poco/Format.h"
#include "Poco/Exception.h"
#include "Poco/Data/Common.h"
#include "Poco/Data/BLOB.h"
#include "Poco/Data/StatementImpl.h"
#include "Poco/Data/ODBC/Connector.h"
#include "Poco/Data/ODBC/Utility.h"
#include "Poco/Data/ODBC/Diagnostics.h"
#include "Poco/Data/ODBC/ODBCException.h"
#include "Poco/Data/ODBC/ODBCStatementImpl.h"
#include <sqltypes.h>
#include <iostream>


using namespace Poco::Data;
using Poco::Data::ODBC::Utility;
using Poco::Data::ODBC::ConnectionException;
using Poco::Data::ODBC::StatementException;
using Poco::Data::ODBC::StatementDiagnostics;
using Poco::format;
using Poco::NotFoundException;

// uncomment to force FreeTDS on Windows
//#define FORCE_FREE_TDS

// uncomment to use native SQL driver
#define POCO_ODBC_USE_SQL_NATIVE

// FreeTDS version selection guide (from http://www.freetds.org/userguide/choosingtdsprotocol.htm)
// (see #define FREE_TDS_VERSION below)
// Product												TDS Version	Comment
// ---------------------------------------------------+------------+------------------------------------------------------------
// Sybase before System 10, Microsoft SQL Server 6.x	4.2			Still works with all products, subject to its limitations.
// Sybase System 10 and above							5.0			Still the most current protocol used by Sybase.
// Sybase System SQL Anywhere							5.0 only 	Originally Watcom SQL Server, a completely separate codebase. 
// 																	Our best information is that SQL Anywhere first supported TDS 
// 																	in version 5.5.03 using the OpenServer Gateway (OSG), and native 
// 																	TDS 5.0 support arrived with version 6.0.
// Microsoft SQL Server 7.0								7.0			Includes support for the extended datatypes in SQL Server 7.0 
// 																	(such as char/varchar fields of more than 255 characters), and 
// 																	support for Unicode.
// Microsoft SQL Server 2000							8.0			Include support for bigint (64 bit integers), variant and collation 
// 																	on all fields. variant is not supported; collation is not widely used. 

#if defined(POCO_OS_FAMILY_WINDOWS) && !defined(FORCE_FREE_TDS)
	#ifdef POCO_ODBC_USE_SQL_NATIVE
		#define MS_SQL_SERVER_ODBC_DRIVER "SQL Native Client"
	#else
		#define MS_SQL_SERVER_ODBC_DRIVER "SQL Server"
	#endif
	#pragma message ("Using " MS_SQL_SERVER_ODBC_DRIVER " driver")
#else
	#define MS_SQL_SERVER_ODBC_DRIVER "FreeTDS"
	#define FREE_TDS_VERSION "8.0"
	#if defined(POCO_OS_FAMILY_WINDOWS)
		#pragma message ("Using " MS_SQL_SERVER_ODBC_DRIVER " driver, version " FREE_TDS_VERSION)
	#endif
#endif


#define MS_SQL_SERVER_DSN "PocoDataSQLServerTest"
#define MS_SQL_SERVER_SERVER "localhost"
#define MS_SQL_SERVER_PORT "1433"
#define MS_SQL_SERVER_DB "test"
#define MS_SQL_SERVER_UID "test"
#define MS_SQL_SERVER_PWD "test"


const bool ODBCSQLServerTest::bindValues[8] = {true, true, true, false, false, true, false, false};
Poco::SharedPtr<Poco::Data::Session> ODBCSQLServerTest::_pSession = 0;
Poco::SharedPtr<SQLExecutor> ODBCSQLServerTest::_pExecutor = 0;
std::string ODBCSQLServerTest::_dsn = "PocoDataSQLServerTest";
std::string ODBCSQLServerTest::_dbConnString;
Poco::Data::ODBC::Utility::DriverMap ODBCSQLServerTest::_drivers;
Poco::Data::ODBC::Utility::DSNMap ODBCSQLServerTest::_dataSources;


ODBCSQLServerTest::ODBCSQLServerTest(const std::string& name): 
	CppUnit::TestCase(name)
{
	static bool beenHere = false;

	ODBC::Connector::registerConnector();
	if (_drivers.empty() || _dataSources.empty()) 
	{
		Utility::drivers(_drivers);
		Utility::dataSources(_dataSources);
		checkODBCSetup();
	}
	if (!_pSession && !_dbConnString.empty() && !beenHere)
	{
		try
		{
			_pSession = new Session(SessionFactory::instance().create(ODBC::Connector::KEY, _dbConnString));
		}catch (ConnectionException& ex)
		{
			std::cout << "!!! WARNING: Connection failed. SQL Server tests will fail !!!" << std::endl;
			std::cout << ex.toString() << std::endl;
		}

		if (_pSession && _pSession->isConnected()) 
			std::cout << "*** Connected to " << _dbConnString << std::endl;
		if (!_pExecutor) _pExecutor = new SQLExecutor("SQLServer SQL Executor", _pSession);
	}
	else 
	if (!_pSession && !beenHere) 
		std::cout << "!!! WARNING: No driver or DSN found. SQL Server tests will fail !!!" << std::endl;

	beenHere = true;
}


ODBCSQLServerTest::~ODBCSQLServerTest()
{
	ODBC::Connector::unregisterConnector();
}


void ODBCSQLServerTest::testBareboneODBC()
{
	if (!_pSession) fail ("Test not available.");

	std::string tableCreateString = "CREATE TABLE Test "
		"(First VARCHAR(30),"
		"Second VARCHAR(30),"
		"Third VARBINARY(30),"
		"Fourth INTEGER,"
		"Fifth FLOAT,"
		"Sixth DATETIME)";

	_pExecutor->bareboneODBCTest(_dbConnString, tableCreateString, 
		SQLExecutor::PB_IMMEDIATE, SQLExecutor::DE_MANUAL, true, "CONVERT(VARBINARY(30),?)");
	_pExecutor->bareboneODBCTest(_dbConnString, tableCreateString, 
		SQLExecutor::PB_IMMEDIATE, SQLExecutor::DE_BOUND, true, "CONVERT(VARBINARY(30),?)");
	_pExecutor->bareboneODBCTest(_dbConnString, tableCreateString, 
		SQLExecutor::PB_AT_EXEC, SQLExecutor::DE_MANUAL, true, "CONVERT(VARBINARY(30),?)");
	_pExecutor->bareboneODBCTest(_dbConnString, tableCreateString, 
		SQLExecutor::PB_AT_EXEC, SQLExecutor::DE_BOUND, true, "CONVERT(VARBINARY(30),?)");
}


void ODBCSQLServerTest::testSimpleAccess()
{
	if (!_pSession) fail ("Test not available.");

	std::string tableName("Person");
	int count = 0;

	recreatePersonTable();

	try { *_pSession << "SELECT count(*) FROM sys.tables WHERE name = 'Person'", into(count), use(tableName), now;  }
	catch(ConnectionException& ce){ std::cout << ce.toString() << std::endl; fail ("testSimpleAccess()"); }
	catch(StatementException& se){ std::cout << se.toString() << std::endl; fail ("testSimpleAccess()"); }

	assert (1 == count);

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->simpleAccess();
		i += 2;
	}
}


void ODBCSQLServerTest::testComplexType()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->complexType();
		i += 2;
	}
}


void ODBCSQLServerTest::testSimpleAccessVector()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->simpleAccessVector();
		i += 2;
	}
}


void ODBCSQLServerTest::testComplexTypeVector()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->complexTypeVector();
		i += 2;
	}	
}


void ODBCSQLServerTest::testInsertVector()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateStringsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->insertVector();
		i += 2;
	}	
}


void ODBCSQLServerTest::testInsertEmptyVector()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateStringsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->insertEmptyVector();
		i += 2;
	}	
}


void ODBCSQLServerTest::testInsertSingleBulk()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateIntsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->insertSingleBulk();
		i += 2;
	}	
}


void ODBCSQLServerTest::testInsertSingleBulkVec()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateIntsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->insertSingleBulkVec();
		i += 2;
	}	
}


void ODBCSQLServerTest::testLimit()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateIntsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->limits();
		i += 2;
	}
}


void ODBCSQLServerTest::testLimitZero()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateIntsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->limitZero();
		i += 2;
	}	
}


void ODBCSQLServerTest::testLimitOnce()
{
	if (!_pSession) fail ("Test not available.");

	recreateIntsTable();
	_pExecutor->limitOnce();
	
}


void ODBCSQLServerTest::testLimitPrepare()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateIntsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->limitPrepare();
		i += 2;
	}
}



void ODBCSQLServerTest::testPrepare()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateIntsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->prepare();
		i += 2;
	}
}


void ODBCSQLServerTest::testSetSimple()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->setSimple();
		i += 2;
	}
}


void ODBCSQLServerTest::testSetComplex()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->setComplex();
		i += 2;
	}
}


void ODBCSQLServerTest::testSetComplexUnique()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->setComplexUnique();
		i += 2;
	}
}

void ODBCSQLServerTest::testMultiSetSimple()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->multiSetSimple();
		i += 2;
	}
}


void ODBCSQLServerTest::testMultiSetComplex()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->multiSetComplex();
		i += 2;
	}	
}


void ODBCSQLServerTest::testMapComplex()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->mapComplex();
		i += 2;
	}
}


void ODBCSQLServerTest::testMapComplexUnique()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->mapComplexUnique();
		i += 2;
	}
}


void ODBCSQLServerTest::testMultiMapComplex()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->multiMapComplex();
		i += 2;
	}
}


void ODBCSQLServerTest::testSelectIntoSingle()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->selectIntoSingle();
		i += 2;
	}
}


void ODBCSQLServerTest::testSelectIntoSingleStep()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->selectIntoSingleStep();
		i += 2;
	}	
}


void ODBCSQLServerTest::testSelectIntoSingleFail()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->selectIntoSingleFail();
		i += 2;
	}	
}


void ODBCSQLServerTest::testLowerLimitOk()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->lowerLimitOk();
		i += 2;
	}	
}


void ODBCSQLServerTest::testSingleSelect()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->singleSelect();
		i += 2;
	}	
}


void ODBCSQLServerTest::testLowerLimitFail()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->lowerLimitFail();
		i += 2;
	}
}


void ODBCSQLServerTest::testCombinedLimits()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->combinedLimits();
		i += 2;
	}
}



void ODBCSQLServerTest::testRange()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->ranges();
		i += 2;
	}
}


void ODBCSQLServerTest::testCombinedIllegalLimits()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->combinedIllegalLimits();
		i += 2;
	}
}



void ODBCSQLServerTest::testIllegalRange()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->illegalRange();
		i += 2;
	}
}


void ODBCSQLServerTest::testEmptyDB()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->emptyDB();
		i += 2;
	}
}


void ODBCSQLServerTest::testBLOB()
{
	if (!_pSession) fail ("Test not available.");
	
	const std::size_t maxFldSize = 250000;
	_pSession->setProperty("maxFieldSize", Poco::Any(maxFldSize-1));
	recreatePersonBLOBTable();

	try
	{
		_pExecutor->blob(maxFldSize, "CONVERT(VARBINARY(MAX),?)");
		fail ("must fail");
	}
	catch (DataException&) 
	{
		_pSession->setProperty("maxFieldSize", Poco::Any(maxFldSize));
	}

	for (int i = 0; i < 8;)
	{
		recreatePersonBLOBTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->blob(maxFldSize, "CONVERT(VARBINARY(MAX),?)");
		i += 2;
	}

	recreatePersonBLOBTable();
	try
	{
		_pExecutor->blob(maxFldSize+1);
		fail ("must fail");
	}
	catch (DataException&) { }
}


void ODBCSQLServerTest::testBLOBStmt()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreatePersonBLOBTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->blobStmt();
		i += 2;
	}
}


void ODBCSQLServerTest::testBool()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateBoolsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->bools();
		i += 2;
	}
}


void ODBCSQLServerTest::testFloat()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateFloatsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->floats();
		i += 2;
	}
}


void ODBCSQLServerTest::testDouble()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateFloatsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->doubles();
		i += 2;
	}
}


void ODBCSQLServerTest::testTuple()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateTuplesTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->tuples();
		i += 2;
	}
}


void ODBCSQLServerTest::testTupleVector()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateTuplesTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->tupleVector();
		i += 2;
	}
}


void ODBCSQLServerTest::testInternalExtraction()
{
	if (!_pSession) fail ("Test not available.");

	for (int i = 0; i < 8;)
	{
		recreateVectorsTable();
		_pSession->setFeature("autoBind", bindValues[i]);
		_pSession->setFeature("autoExtract", bindValues[i+1]);
		_pExecutor->internalExtraction();
		i += 2;
	}
}


void ODBCSQLServerTest::dropTable(const std::string& tableName)
{
	try
	{
		*_pSession << format("DROP TABLE %s", tableName), now;
	}
	catch (StatementException& ex)
	{
		bool ignoreError = false;
		const StatementDiagnostics::FieldVec& flds = ex.diagnostics().fields();
		StatementDiagnostics::Iterator it = flds.begin();
		for (; it != flds.end(); ++it)
		{
			if (3701 == it->_nativeError)//(table does not exist)
			{
				ignoreError = true;
				break;
			}
		}

		if (!ignoreError) 
		{
			std::cout << ex.displayText() << std::endl;
			throw;
		}
	}
}


void ODBCSQLServerTest::recreatePersonTable()
{
	dropTable("Person");
	try { *_pSession << "CREATE TABLE Person (LastName VARCHAR(30), FirstName VARCHAR(30), Address VARCHAR(30), Age INTEGER)", now; }
	catch(ConnectionException& ce){ std::cout << ce.toString() << std::endl; fail ("recreatePersonTable()"); }
	catch(StatementException& se){ std::cout << se.toString() << std::endl; fail ("recreatePersonTable()"); }
}


void ODBCSQLServerTest::recreatePersonBLOBTable()
{
	dropTable("Person");
	try { *_pSession << "CREATE TABLE Person (LastName VARCHAR(30), FirstName VARCHAR(30), Address VARCHAR(30), Image VARBINARY(MAX))", now; }
	catch(ConnectionException& ce){ std::cout << ce.toString() << std::endl; fail ("recreatePersonBLOBTable()"); }
	catch(StatementException& se){ std::cout << se.toString() << std::endl; fail ("recreatePersonBLOBTable()"); }
}


void ODBCSQLServerTest::recreateIntsTable()
{
	dropTable("Strings");
	try { *_pSession << "CREATE TABLE Strings (str INTEGER)", now; }
	catch(ConnectionException& ce){ std::cout << ce.toString() << std::endl; fail ("recreateIntsTable()"); }
	catch(StatementException& se){ std::cout << se.toString() << std::endl; fail ("recreateIntsTable()"); }
}


void ODBCSQLServerTest::recreateStringsTable()
{
	dropTable("Strings");
	try { *_pSession << "CREATE TABLE Strings (str VARCHAR(30))", now; }
	catch(ConnectionException& ce){ std::cout << ce.toString() << std::endl; fail ("recreateStringsTable()"); }
	catch(StatementException& se){ std::cout << se.toString() << std::endl; fail ("recreateStringsTable()"); }
}


void ODBCSQLServerTest::recreateBoolsTable()
{
	dropTable("Strings");
	try { *_pSession << "CREATE TABLE Strings (str BIT)", now; }
	catch(ConnectionException& ce){ std::cout << ce.toString() << std::endl; fail ("recreateBoolsTable()"); }
	catch(StatementException& se){ std::cout << se.toString() << std::endl; fail ("recreateBoolsTable()"); }
}


void ODBCSQLServerTest::recreateFloatsTable()
{
	dropTable("Strings");
	try { *_pSession << "CREATE TABLE Strings (str FLOAT)", now; }
	catch(ConnectionException& ce){ std::cout << ce.toString() << std::endl; fail ("recreateFloatsTable()"); }
	catch(StatementException& se){ std::cout << se.toString() << std::endl; fail ("recreateFloatsTable()"); }
}


void ODBCSQLServerTest::recreateTuplesTable()
{
	dropTable("Tuples");
	try { *_pSession << "CREATE TABLE Tuples "
		"(int0 INTEGER, int1 INTEGER, int2 INTEGER, int3 INTEGER, int4 INTEGER, int5 INTEGER, int6 INTEGER, "
		"int7 INTEGER, int8 INTEGER, int9 INTEGER, int10 INTEGER, int11 INTEGER, int12 INTEGER, int13 INTEGER,"
		"int14 INTEGER, int15 INTEGER, int16 INTEGER, int17 INTEGER, int18 INTEGER, int19 INTEGER)", now; }
	catch(ConnectionException& ce){ std::cout << ce.toString() << std::endl; fail ("recreateTuplesTable()"); }
	catch(StatementException& se){ std::cout << se.toString() << std::endl; fail ("recreateTuplesTable()"); }
}


void ODBCSQLServerTest::recreateVectorTable()
{
	dropTable("Vector");
	try { *_pSession << "CREATE TABLE Vector (i0 INTEGER)", now; }
	catch(ConnectionException& ce){ std::cout << ce.toString() << std::endl; fail ("recreateVectorTable()"); }
	catch(StatementException& se){ std::cout << se.toString() << std::endl; fail ("recreateVectorTable()"); }
}


void ODBCSQLServerTest::recreateVectorsTable()
{
	dropTable("Vectors");
	try { *_pSession << "CREATE TABLE Vectors (int0 INTEGER, flt0 FLOAT, str0 VARCHAR(30))", now; }
	catch(ConnectionException& ce){ std::cout << ce.toString() << std::endl; fail ("recreateVectorsTable()"); }
	catch(StatementException& se){ std::cout << se.toString() << std::endl; fail ("recreateVectorsTable()"); }
}


void ODBCSQLServerTest::checkODBCSetup()
{
	static bool beenHere = false;

	if (!beenHere)
	{
		beenHere = true;
		
		bool driverFound = false;
		bool dsnFound = false;

		Utility::DriverMap::iterator itDrv = _drivers.begin();
		for (; itDrv != _drivers.end(); ++itDrv)
		{
			if (((itDrv->first).find(MS_SQL_SERVER_ODBC_DRIVER) != std::string::npos))
			{
				std::cout << "Driver found: " << itDrv->first 
					<< " (" << itDrv->second << ')' << std::endl;
				driverFound = true;
				break;
			}
		}

		if (!driverFound) 
		{
			std::cout << "SQL Server driver NOT found, tests will fail." << std::endl;
			return;
		}
		
		Utility::DSNMap::iterator itDSN = _dataSources.begin();
		for (; itDSN != _dataSources.end(); ++itDSN)
		{
			if (((itDSN->first).find(_dsn) != std::string::npos) &&
				(((itDSN->second).find(MS_SQL_SERVER_ODBC_DRIVER) != std::string::npos)))
			{
				std::cout << "DSN found: " << itDSN->first 
					<< " (" << itDSN->second << ')' << std::endl;
				dsnFound = true;
				break;
			}
		}

		if (!dsnFound) 
		{
			if (!_pSession && _dbConnString.empty())
			{
				std::cout << "MS SQL Server DSN NOT found, will attempt to connect without it." << std::endl;
				_dbConnString = "DRIVER=" MS_SQL_SERVER_ODBC_DRIVER ";"
					"UID=" MS_SQL_SERVER_UID ";"
					"PWD=" MS_SQL_SERVER_PWD ";"
					"DATABASE=" MS_SQL_SERVER_DB ";"
					"SERVER=" MS_SQL_SERVER_SERVER ";"
					"PORT=" MS_SQL_SERVER_PORT ";"
#ifdef FREE_TDS_VERSION
					"TDS_Version=" FREE_TDS_VERSION ";"
#endif
					;
				return;
			}
			else if (!_dbConnString.empty())
			{
				std::cout << "MS SQL Server tests not available." << std::endl;
				return;
			}
		}
	}

	if (!_pSession)
		format(_dbConnString, "DSN=%s;Uid=test;Pwd=test;", _dsn);
}


void ODBCSQLServerTest::setUp()
{
}


void ODBCSQLServerTest::tearDown()
{
}


CppUnit::Test* ODBCSQLServerTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ODBCSQLServerTest");

	CppUnit_addTest(pSuite, ODBCSQLServerTest, testBareboneODBC);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testSimpleAccess);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testComplexType);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testSimpleAccessVector);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testComplexTypeVector);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testInsertVector);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testInsertEmptyVector);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testInsertSingleBulk);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testInsertSingleBulkVec);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testLimit);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testLimitOnce);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testLimitPrepare);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testLimitZero);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testPrepare);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testSetSimple);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testSetComplex);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testSetComplexUnique);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testMultiSetSimple);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testMultiSetComplex);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testMapComplex);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testMapComplexUnique);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testMultiMapComplex);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testSelectIntoSingle);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testSelectIntoSingleStep);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testSelectIntoSingleFail);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testLowerLimitOk);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testLowerLimitFail);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testCombinedLimits);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testCombinedIllegalLimits);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testRange);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testIllegalRange);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testSingleSelect);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testEmptyDB);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testBLOB);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testBLOBStmt);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testBool);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testFloat);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testDouble);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testTuple);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testTupleVector);
	CppUnit_addTest(pSuite, ODBCSQLServerTest, testInternalExtraction);

	return pSuite;
}
