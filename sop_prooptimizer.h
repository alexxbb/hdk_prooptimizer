#ifndef _PROOPTIMIZER
#define _PROOPTIMIZER


#include <UT/UT_DSOVersion.h>
#include <OP/OP_Node.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Parameters.h>




class ProOptimizer : public SOP_Node
{
public:
	ProOptimizer(OP_Network *net, const char *name, OP_Operator *op);
	virtual ~ProOptimizer();

	static OP_Node *MyConstructor(OP_Network*, const char*, OP_Operator *);
	static PRM_Template parmsTemplatesList[];

protected:
	OP_ERROR cookMySop(OP_Context &context);
	virtual bool updateParmsFlags();


private:
	void PARM_CAM_PATH(UT_String &val, fpreal t) { evalString(val, "cam_path", 0, t); } 
	void PARM_SHOP_PROPERTIES(UT_String &val, fpreal t) { evalString(val, "properties_shop_path", 0, t); }
	int PARM_CREATE_OFFSCREEN_GRP() { return evalInt("createOffscreenGroup", 0, 0, 1); }
	int PARM_DELETE_PRIMS() { return evalInt("deleteOffscreenPrims", 0, 0, 1); }
	int PARM_OVERRIDE_SHOP() { return evalInt("overrideProperties", 0, 0, 0); }

	void EVAL_SHOP_PROPERTIES(SHOP_Node *propNode, fpreal t);

	UT_WorkBuffer shopProperties;
};

#endif