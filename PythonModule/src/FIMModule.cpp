/* 
 *  File: FIMModule.cpp
 *  Copyright (c) 2020 Florian Porrmann
 *  
 *  MIT License
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 */

#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdio.h>
#include <string>
#include <sys/resource.h>
#include <unistd.h>

#include <Python.h>

#include "FPGrowth.h"
#include "Logger.h"
#include "Utils.h"

#define MAKE_NAME(x)      PyInit_##x
#define INIT_FUNC_NAME(x) MAKE_NAME(x)

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#define ERR_TYPE(s)                          \
	{                                        \
		sigRemove();                         \
		PyErr_SetString(PyExc_TypeError, s); \
	}
#define ERR_MEM(s)                             \
	{                                          \
		sigRemove();                           \
		PyErr_SetString(PyExc_MemoryError, s); \
	}
#define ERR_ABORT()                                        \
	{                                                      \
		sigRemove();                                       \
		PyErr_SetString(PyExc_RuntimeError, "user abort"); \
	}

#define MAJOR_VERSION 0
#define MINOR_VERSION 2
#define PATCH_VERSION 0

#define VERSION              \
	TO_STRING(MAJOR_VERSION) \
	"." TO_STRING(MINOR_VERSION) "." TO_STRING(PATCH_VERSION)

DEFINE_EXCEPTION(ModuleException)

// =========  Python Module Setup  ======== //

PyObject* fpgrowth(PyObject* self, PyObject* args, PyObject* kwds);

static PyMethodDef ModuleFunctions[] = {
	{ "fpgrowth", reinterpret_cast<PyCFunction>(fpgrowth), METH_VARARGS | METH_KEYWORDS, nullptr },
	{ nullptr, nullptr, 0, nullptr }
};

// Disable the missing-field-initializers warning as some
// sub states of PyModuleDef won't be initialized here
#if !defined(_MSC_VER) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

// Module definition
static struct PyModuleDef ModuleDefinitions = {
	PyModuleDef_HEAD_INIT,
	TO_STRING(MODULE_NAME), // Name of the Module
	// Module documentation (docstring)
	"C++-based FPGrowth implementation for python3.6",
	-1,
	ModuleFunctions // Functions exposed to the module
};

#if !defined(_MSC_VER) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

PyMODINIT_FUNC INIT_FUNC_NAME(MODULE_NAME)(void)
{
	Py_Initialize();
	PyObject* pModule = PyModule_Create(&ModuleDefinitions);
	PyModule_AddObject(pModule, "version", Py_BuildValue("s", "version 0.1-Alpha"));
	return pModule;
}

// =========  Utility Functions  ======== //

PyObject* long2PyLong(const long& val)
{
	PyObject* pyVal = PyLong_FromLong(val);
	if (!pyVal) throw(ModuleException("Unable to allocate memory for Python Long element"));
	return pyVal;
}

PyObject* createPyList(const size_t& size = 0)
{
	PyObject* pyList = PyList_New(size);
	if (!pyList)
		throw(ModuleException(string_format("Unable to allocate memory for Python List with %lld elements", size)));

	return pyList;
}

PyObject* createPyTuple(const size_t& size = 0)
{
	PyObject* pyTuple = PyTuple_New(size);
	if (!pyTuple)
		throw(ModuleException(string_format("Unable to allocate memory for Python Tuple with %lld elements", size)));

	return pyTuple;
}

void cleanupPyRefs(std::initializer_list<PyObject*> objs)
{
	for (PyObject* pObj : objs)
		Py_DECREF(pObj);
}

// =========  Python Module Functions  ======== //

static constexpr ItemC WIN_LEN = 20;

PyObject* fpgrowth(PyObject* self, PyObject* args, PyObject* kwds)
{
	UNUSED(self);
	const char* ckwds[] = { "tracts", "target", "supp", "zmin", "zmax", "report", "algo", "winlen", "verbose", nullptr };
	PyObject* tracts;
	char* target    = nullptr;
	double supp     = 10;
	Support support = 0;
	uint32_t zmin   = 1;
	uint32_t zmax   = 0;
	char* report    = nullptr;
	char* algo      = nullptr;
	uint32_t winlen = WIN_LEN;
	int32_t verbose = ToUnderlying(Verbosity::VB_INFO);
	Verbosity verbosity;
	Timer fullTimer;

	std::map<Py_hash_t, PyObject*> hashMap;

	fullTimer.Start();

	// ===== Evaluate the Function Arguments ===== //
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|sdIIssII", const_cast<char**>(ckwds), &tracts, &target, &supp, &zmin, &zmax, &report, &algo, &winlen, &verbose))
		return nullptr;

	support   = static_cast<Support>(std::abs(supp));
	verbosity = ToVerbosity(verbose);

	SetVerbosity(verbosity);

	LOG_INFO << " =========  FPGrowth C++ Module (v" VERSION ") - Start - PID: " << getpid() << "  ========= " << std::endl;

	sigInstall(); // Install signal handler to catch CTRL-C interrupts

	// ========= Load Transaction Database from Python START ========= //
	PyObject* pTractsItr = PyObject_GetIter(tracts);

	if (!pTractsItr)
	{
		ERR_TYPE("transaction database must be iterable");
		return nullptr;
	}

	PyObject* pTransItr;
	PyObject* pItemItr;
	PyObject* pItem;
	Transactions transactions;

	while ((pTransItr = PyIter_Next(pTractsItr)) != nullptr)
	{
		pItemItr = PyObject_GetIter(pTransItr);
		cleanupPyRefs({ pTransItr });

		if (!pItemItr)
		{
			cleanupPyRefs({ pTractsItr });
			ERR_TYPE("transactions must be iterable");
			return nullptr;
		}

		Transaction tc;
		while ((pItem = PyIter_Next(pItemItr)) != nullptr)
		{
			Py_hash_t h = PyObject_Hash(pItem);
			if (h == -1)
			{
				cleanupPyRefs({ pItem, pItemItr, pTractsItr });
				ERR_TYPE("items must be hashable");
				return nullptr;
			}

			hashMap.try_emplace(h, pItem);

			// TODO: For non 32-bit values this will result in problems
			tc.push_back(static_cast<ItemC>(h));

			cleanupPyRefs({ pItem });
		}

		transactions.push_back(tc);
		cleanupPyRefs({ pItemItr });
	}

	cleanupPyRefs({ pTractsItr });

	// ========= Load Transaction Database from Python END ========= //

	std::vector<PatternPair> closed;

	try
	{
		FPGrowth fp(transactions, support, zmin, zmax);
		const Pattern* pPattern = fp.Growth();

		std::cout << "Memory Usage after FPGrowth: " << GetMemString() << std::endl;

		std::vector<const PatternType*> processed;
		//PostProcessing(pPattern, transactions.size(), fp.GetItemCount(), zmin, winlen, fp.GetId2Item(), processed);

		std::cout << "Memory Usage after Post Processiong: " << GetMemString() << std::endl;

		ClosedDetection(fp, pPattern, closed);
	}
	catch (const FPGException& e)
	{
		sigAbort(0);
		PyErr_SetInterrupt();
		ERR_ABORT();
		return nullptr;
	}

	std::cout << "Converting Pattern to Python List ... " << std::flush;
	Timer t;
	t.Start();

	try
	{
		PyObject* pyList = createPyList(closed.size());
		PyObject* pyPatternWSupp;
		PyObject* pyPattern;

		for (auto [idx, pp] : enumerate(closed))
		{
			pyPatternWSupp = createPyTuple(2);
			pyPattern      = createPyTuple(pp.first.size());

			for (auto [i, item] : enumerate(pp.first))
			{
				PyObject* pItem = hashMap[item];
				Py_INCREF(pItem);
				PyTuple_SET_ITEM(pyPattern, i, pItem);
			}

			PyTuple_SET_ITEM(pyPatternWSupp, 0, pyPattern);              // Set Pattern
			PyTuple_SET_ITEM(pyPatternWSupp, 1, long2PyLong(pp.second)); // Set Support

			PyList_SET_ITEM(pyList, idx, pyPatternWSupp);
		}

		t.Stop();
		std::cout << "Done after: " << t << std::endl;
		std::cout << "Memory Usage after Conmversion: " << GetMemString() << std::endl;

		fullTimer.Stop();
		std::cout << " =========  FPGrowth C++ Module End (" << fullTimer << ")  ========= " << std::endl;

		sigRemove();
		return pyList;
	}
	catch (const ModuleException& e)
	{
		ERR_MEM(e.what())
		return nullptr;
	}
}
