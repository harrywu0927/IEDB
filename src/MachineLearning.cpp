#include <utils.hpp>

int DB_OutlierDetection(DB_DataBuffer *buffer, DB_QueryParams *params)
{
    if (!TemplateManager::CheckTemplate(params->pathToLine))
        return StatusCode::SCHEMA_FILE_NOT_FOUND;
    auto allTypes = CurrentTemplate.GetAllTypes(params->pathCode);
    int err = DB_ExecuteQuery(buffer, params);
    if (err != 0)
        return err;
    Py_Initialize();
    PyObject *arr = PyList_New(100);
    PyObject *lstitem;
    for (int i = 0; i < 100; i++)
    {
        lstitem = PyLong_FromLong(i % 20 == 0 ? rand() % 100 : rand() % 10);
        PyList_SetItem(arr, i, lstitem);
    }

    PyObject *obj;
    // 指定py文件目录
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append('./')");

    // PyObject *pname = Py_BuildValue("s", "testpy");
    PyObject *mymodule = PyImport_ImportModule("Novelty_Outlier");
    // PyObject *numpy = PyImport_ImportModule("numpy");
    // PyObject *std = PyObject_GetAttrString(numpy, "fft");
    PyObject *pValue, *pArgs, *pFunc;
    long res = 0;
    if (mymodule != NULL)
    {
        // 从模块中获取函数
        pFunc = PyObject_GetAttrString(mymodule, "Outliers");

        if (pFunc && PyCallable_Check(pFunc))
        {
            // 创建参数元组
            pArgs = PyTuple_New(2);
            PyTuple_SetItem(pArgs, 0, arr);
            PyTuple_SetItem(pArgs, 1, PyLong_FromLong(1));
            // for (int i = 0; i < 2; ++i)
            // {
            //     // 设置参数值
            //     pValue = PyLong_FromLong(i + 10);
            //     PyTuple_SetItem(pArgs, i, pValue);
            // }
            // 函数执行
            PyObject *ret = PyObject_CallObject(pFunc, pArgs);
            PyObject *item;
            long val;
            int len = PyObject_Size(ret);
            for (int i = 0; i < len; i++)
            {
                item = PyList_GetItem(ret, i); //根据下标取出python列表中的元素
                val = PyLong_AsLong(item);     //转换为c类型的数据
                cout << val << " ";
            }
            // res = PyLong_AsLong(PyList_GetItem(pValue, 1));
            // cout << pValue->ob_type->tp_name << endl;
        }
    }
    Py_Finalize();
}
// int main()
// {
//     return 0;
// }