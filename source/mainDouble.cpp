/*
	This source code implements a Finite Volume Method for discretization and solution of the 
	consolidation problem . The governing equations are discretized within the FVM and the resulting
	linear system of equations is solved with a LU Factorization found in PETSc [1]. The parameters 
	chosen are such that the stability of the solution is tested.

 	Written by FERREIRA, C. A. S.

 	Florianópolis, 2020.
	
 	[1] BALAY et al. PETSc User Manual. Technical Report, Argonne National Laboratory, 2017.
*/

#include "customPrinter.hpp"
#include "exportRunInfo.hpp"
#include "benchmarking.hpp"

int main(int argc, char** args)
{	
	string myGridType=args[1];
	string myInterpScheme=args[2];
	string myMedium=args[3];

/*		PROPERTIES IMPORT
	----------------------------------------------------------------*/	

	poroelasticProperties myProperties;
	ifstream inFile;
	inFile.open("../input/"+myMedium+".txt");
	if(!inFile)
	{
		cout << "Unable to open properties file.";
		exit(1);
	}
	getline(inFile,myProperties.pairName);
	myProperties.pairName=myMedium;
	inFile >> myProperties.shearModulus;
	inFile >> myProperties.bulkModulus;
	inFile >> myProperties.solidBulkModulus;
	inFile >> myProperties.solidDensity;
	inFile >> myProperties.fluidBulkModulus;
	inFile >> myProperties.porosity;
	inFile >> myProperties.permeability;
	inFile >> myProperties.fluidViscosity;
	inFile >> myProperties.fluidDensity;
	inFile.close();	
	myProperties.macroPorosity=myProperties.porosity*1./3.;
	myProperties.porosity=myProperties.porosity*2./3.;
	myProperties.macroPermeability=myProperties.permeability*999./1000.;
	myProperties.permeability=myProperties.permeability/1000.;
	
/*		GRID DEFINITION
	----------------------------------------------------------------*/

	// Consolidation coefficient
	double storativity,porosity,fluidViscosity,permeability,fluidCompressibility,
		solidCompressibility,bulkCompressibility,longitudinalModulus,alpha;
	porosity=myProperties.macroPorosity;
	fluidViscosity=myProperties.fluidViscosity;
	permeability=myProperties.macroPermeability;
	fluidCompressibility=1/myProperties.fluidBulkModulus;
	solidCompressibility=1/myProperties.solidBulkModulus;
	bulkCompressibility=1/myProperties.bulkModulus;
	longitudinalModulus=myProperties.bulkModulus+4*myProperties.shearModulus/3;
	alpha=1-solidCompressibility/bulkCompressibility;
	storativity=porosity*fluidCompressibility+(alpha-porosity)*solidCompressibility;
	double consolidationCoefficient=(permeability/fluidViscosity)/(storativity+
		alpha*alpha/longitudinalModulus);

	int Nt=2;
	int mesh=3;
	double h=1./mesh;
	double consolidationTime=h*h/consolidationCoefficient;
	double Lt;
	double dt;
	vector<double> timestepSize=
	{
		0.25,
		0.10
	};

/*		OTHER PARAMETERS
	----------------------------------------------------------------*/

	double g=0; // m/s^2
	double columnLoad=-10e3; // Pa
	
/*		PETSC INITIALIZE
	----------------------------------------------------------------*/

	PetscErrorCode ierr;
	ierr=PetscInitialize(&argc,&args,(char*)0,NULL);CHKERRQ(ierr);

/*		SOLVE BENCHMARKING PROBLEMS
	----------------------------------------------------------------*/

	cout << "Grid type: " << myGridType << "\n";
	cout << "Interpolation scheme: " << myInterpScheme << "\n";
	cout << "Minimum time-step: " << consolidationTime/6 << "\n";
	cout << "Solved Terzaghi (double-porosity) for: \n";
	createSolveRunInfo(myGridType,myInterpScheme,"Terzaghi");
	for(int i=0; i<timestepSize.size(); i++)
	{
		Lt=(Nt-1)*(consolidationTime*timestepSize[i]);
		dt=Lt/(Nt-1);
		exportSolveRunInfo(dt,"Terzaghi_"+myMedium);
		ierr=terzaghiDouble(myGridType,myInterpScheme,Nt,mesh,Lt,g,columnLoad,myProperties);
			CHKERRQ(ierr);
	}
	
/*		PETSC FINALIZE
	----------------------------------------------------------------*/
	
	ierr=PetscFinalize();CHKERRQ(ierr);

	return ierr;
};