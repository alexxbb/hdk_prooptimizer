#include <OP/OP_Context.h>
#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_Camera.h>
#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_GeoOverride.h>
#include <OP/OP_OperatorTable.h>
#include <GU/GU_Detail.h>
#include <GA/GA_Detail.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_Type.h>
#include <PRM/PRM_SpareData.h>
#include <PRM/PRM_Error.h>
#include <PRM/PRM_Parm.h>
#include <UT/UT_Matrix4.h>
#include "sop_prooptimizer.h"

OP_Node *ProOptimizer::MyConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
	return new ProOptimizer(net, name, op);
}

ProOptimizer::ProOptimizer(OP_Network *net, const char *name, OP_Operator *op)
:SOP_Node(net, name, op)
{
	//mySopFlags.setNeedGuide1(1);
}

ProOptimizer::~ProOptimizer() {};


static PRM_Name parmNames[] = 
{
	PRM_Name("cam_path", "Camera"),
	PRM_Name("overrideProperties", "Override Render Properties"),
	PRM_Name("properties_shop_path", "Properties SHOP"),
	PRM_Name("createOffscreenGroup", "Create Offscreen Prim Group"),
	PRM_Name("deleteOffscreenPrims", "Delete Offscreen Prims"),
};

static PRM_Default defaults[] =
{
	PRM_Default (2, "Shading"),
	PRM_Default (2, "OffScreen"),
};

PRM_Template
ProOptimizer::parmsTemplatesList[] = 
{
	// Camera
	PRM_Template(PRM_STRING_OPREF, PRM_TYPE_DYNAMIC_PATH,
	1, &parmNames[0], 0, 0, 0, 0, &PRM_SpareData::objCameraPath),
	
	PRM_Template(PRM_SWITCHER , 2, &PRMswitcherName, defaults),
	// Shading Tab
	PRM_Template(PRM_TOGGLE, 1, &parmNames[1]),
	PRM_Template(PRM_STRING_OPREF, PRM_TYPE_DYNAMIC_PATH,
	1, &parmNames[2], 0, 0, 0, 0, &PRM_SpareData::shopProperties),

	// Offsreen Tab
	PRM_Template(PRM_TOGGLE, 1, &parmNames[3]),
	PRM_Template(PRM_TOGGLE, 1, &parmNames[4]),

	PRM_Template(),
};

bool
ProOptimizer::updateParmsFlags()
{
	bool changes = false;

	changes |= enableParm("properties_shop_path", PARM_OVERRIDE_SHOP());
	changes |= enableParm("deleteOffscreenPrims", PARM_CREATE_OFFSCREEN_GRP());
	return changes;
}

void
ProOptimizer::EVAL_SHOP_PROPERTIES(SHOP_Node *propNode, fpreal t)
{
	SHOP_GeoOverride geoOverride;
	const PRM_ParmList *parmList = propNode->getParmList();
	for(int i=0; i < parmList->getEntries(); i++)
	{
		const PRM_Parm *curParm = parmList->getParmPtr(i);
		const char *parmName = curParm->getToken();
		const PRM_Type parmType = curParm->getType();
		if (parmType & PRM_TYPE_FLOAT)
		{
			fpreal parmValue;
			curParm->getValue(t, parmValue, 0, 1);
			geoOverride.addKey(parmName, parmValue);
		}
		else if((parmType & PRM_TYPE_INTEGER) || parmType & PRM_TYPE_TOGGLE)
		{
			exint parmValue;
			curParm->getValue(t, parmValue, 0, 1);
			geoOverride.addKey(parmName, parmValue);
		}
		else if(parmType & PRM_TYPE_STRING)
		{
			UT_String parmValue;
			curParm->getValue(t, parmValue, 0, true, 1);
			geoOverride.addKey(parmName, parmValue);
		}
		else
			continue;
	}
	geoOverride.save(shopProperties);
}


bool
pointOnScreen(UT_Vector3 *screenP)
{
	if (screenP->x() > 1.0 || screenP->x() < 0 ||
		screenP->y() > 1.0 || screenP->y() < 0)
		return false;
	else
		return true;
}


OP_ERROR
ProOptimizer::cookMySop(OP_Context &context)
{
	flags().timeDep = 1;
	fpreal now = context.getTime();
	if (lockInputs(context) >= UT_ERROR_ABORT)
	{
		return error();
	}
	duplicateSource(0, context);
	
	////// Parameter Evaluation ////////
	UT_String camera_name;
	UT_String shop_path;
	PARM_CAM_PATH(camera_name, now);
	PARM_SHOP_PROPERTIES(shop_path, now);
	int overrideShop = PARM_OVERRIDE_SHOP();
	int deleteOffscreenPrims = PARM_DELETE_PRIMS();
	int createOffsceenGroup = PARM_CREATE_OFFSCREEN_GRP();
	OBJ_Node *container = (OBJ_Node *) getParent();
	OBJ_Camera  *cameraptr = (OBJ_Camera *) findNode(camera_name);
	//cameraptr = (OBJ_Camera *) ;
	if (!cameraptr)
	{
		addError(SOP_ERR_INVALID_SRC, "Please select camera object");
		return error();
	}

	// Add properties shop path and material override attrib
	GA_RWAttributeRef propsShopARef;
	GA_RWAttributeRef matOverrideARef;
	GA_RWHandleS shopPathHandle;
	GA_RWHandleS matOverrideHandle;
	if (overrideShop)
	{
		SHOP_Node *propertiesShop = CAST_SHOPNODE(findNode(shop_path)); // or can be findSHOPNode(path)
		if (!propertiesShop)
		{
			addError(SOP_ERR_INVALID_SRC, "Please select properties");
			return error();
		}
		
		EVAL_SHOP_PROPERTIES(propertiesShop, now);
		propsShopARef = gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "shop_materialpath", 1);
		shopPathHandle = propsShopARef.getAttribute();

		matOverrideARef = gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "material_override", 1);
		matOverrideHandle = matOverrideARef.getAttribute();
	}

	
	float resx = cameraptr->evalFloat("res", 0, now);
	float resy = cameraptr->evalFloat("res", 1, now);
	float image_aspect = resx/resy;


	UT_Matrix4D inverseCameraMatrix;
	UT_Matrix4D projectionMatrix;
	UT_Matrix4 objLocalTransform;
	container->getLocalTransform(context, objLocalTransform);
	cameraptr->getInverseLocalToWorldTransform(context, inverseCameraMatrix);
	cameraptr->getProjectionMatrix(context, projectionMatrix);

	UT_Matrix4D objectToNDC = inverseCameraMatrix * projectionMatrix;


	// Groups for screen/offscreen primitives
	GA_PrimitiveGroup *outPrimGrp = 0;
	GA_PrimitiveGroup *screenGroup = 0;
	outPrimGrp = gdp->newPrimitiveGroup("offscreen_prims", (1 - createOffsceenGroup));
	screenGroup = gdp->newPrimitiveGroup("screen_prims", 1);

	///// Project geometry to screen
	UT_Vector4 P_ndc;
	UT_Vector3 P_screen;
	GA_RWHandleV4 pHandle(gdp->getP());
	GA_OffsetArray primsOffsets;
	for(GA_Iterator it(gdp->getPointRange()); !it.atEnd(); it.advance())
	{
		GA_Offset ptoffset = it.getOffset();
		P_ndc = pHandle.get(ptoffset) * objectToNDC;
		P_ndc /= P_ndc.w();
		P_screen = (UT_Vector3) P_ndc;
		P_screen *= 0.5;
		P_screen.assign(P_screen.x(), P_screen.y() *= image_aspect, P_screen.z());
		P_screen += UT_Vector3(0.5, 0.5, 0.0);
		if (pointOnScreen(&P_screen))
		{
			//std::cout << "Point: " << ptoffset << " is offscreen" << std::endl;
			gdp->getPrimitivesReferencingPoint(primsOffsets, ptoffset);
			GA_OffsetArray::const_iterator primIt;
			for(primIt = primsOffsets.begin(); !primIt.atEnd(); primIt.advance())
			{
				if (!screenGroup->containsOffset(*primIt))
					screenGroup->addOffset(*primIt);
					//std::cout << gdp->primitiveIndex(*primIt) << std::endl;
			}

		}//end if
	
	}//end for
	
	for(GA_Iterator grp_it(gdp->getPrimitiveRange()); !grp_it.atEnd(); ++grp_it)
	{
		GA_Offset primOffset = grp_it.getOffset();
		if (!screenGroup->containsOffset(primOffset))
		{
			outPrimGrp->addOffset(primOffset);
			if (overrideShop)
			{
				shopPathHandle.set(primOffset, shop_path);
				matOverrideHandle.set(primOffset, shopProperties.buffer());
			}
			
		}
		
	}

	if (deleteOffscreenPrims)
		gdp->deletePrimitives(*outPrimGrp, true);

	unlockInputs();
	return error();

}

void
newSopOperator(OP_OperatorTable *table)
{
	table->addOperator(
		new OP_Operator("pro_optimizer",
						"ProOptimizer",
						ProOptimizer::MyConstructor,
						ProOptimizer::parmsTemplatesList,
						1,
						1)
						);
}
