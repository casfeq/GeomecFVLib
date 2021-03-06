/*
	This header is part of the development of a master's thesis entitled "Analysis of Numerical
	Schemes in Collocated and Staggered Grids for Problems of Poroelasticity". The functions here
	defined uses the classes predefined for the solution of the benchmarking problems, presented and
	solved by Terzaghi [2] and Mandel [1], and for the convergence analysis of the method.

 	Written by FERREIRA, C. A. S.

 	Florianópolis, 2019.

 	[1] MANDEL, J. Consolidation Des Sols (Étude Mathématique). Géotechnique, v. 3, n. 7, pp. 287-
	299, 1953.
	[2] TERZAGHI, K. Erdbaumechanik auf Bodenphysikalischer Grundlage. Franz Deuticke, Leipzig,
 	1925.
*/

#include "gridDesign.hpp"
#include "problemParameters.hpp"
#include "problemDoubleParameters.hpp"
#include "coefficientsAssembly.hpp"
#include "independentTermsAssembly.hpp"
#include "linearSystemSolver.hpp"
#include "dataProcessing.hpp"
#include "doubleDataProcessing.hpp"

struct poroelasticProperties
{
	// Pair name
	string pairName;

	// Bulk parameters
	double shearModulus;
	double bulkModulus;
	double porosity;
	double permeability;
	double macroPorosity;
	double macroPermeability;

	// Solid parameters
	double solidBulkModulus;
	double solidDensity;

	// Fluid parameters
	double fluidBulkModulus;
	double fluidViscosity;
	double fluidDensity;
};

int sealedColumn(string gridType, string interpScheme, int Nt, int meshSize, double Lt, double g,
	double sigmab, poroelasticProperties myProperties)
{
	PetscErrorCode ierr;

/*		ENTRY PARAMETERS
	----------------------------------------------------------------*/

	// Grid parameters
	int Nx=meshSize;
	int Ny=6*meshSize;

	// Reservoir parameters
	double Lx=1; // [m]
	double Ly=6; // [m]

	vector<vector<double>> sCoordinates=
	{
		{Lx,Ly},
		{0,Ly},
		{0,0},
		{Lx,0}
	};

	// Bulk properties
	string pairName=myProperties.pairName;
	double G=myProperties.shearModulus;
	double lambda=myProperties.bulkModulus-2*G/3;
	double phi=myProperties.porosity;
	double K=myProperties.permeability;

	// Solid properties
	double c_s=1/myProperties.solidBulkModulus;
	double rho_s=myProperties.solidDensity;

	// Fluid properties
	double c_f=1/myProperties.fluidBulkModulus;
	double rho_f=myProperties.fluidDensity;
	double mu_f=myProperties.fluidViscosity;

	// BC types ({u,v,P} 1 for Dirichlet and 0 for Neumann, -1 for Stress/Fluid Flow, starts on
	// "north" and follows counterclockwise)
	vector<vector<int>> bcType=
	{
		{-1,-1,0},
		{1,-1,-1},
		{-1,1,0},
		{1,-1,-1}
	};

	// BC values ({u,v,P}, starts on "north" and follows counterclockwise)
	vector<vector<double>> bcValue=
	{
		{0,sigmab,rho_f*g},
		{0,0,0},
		{0,0,rho_f*g},
		{0,0,0}
	};

/*		GRID CREATION
	----------------------------------------------------------------*/

	// Constructor
	gridDesign myGrid(Nx,Ny,Nt,Lx,Ly,Lt,gridType,sCoordinates);

	// Passing variables
	int Nu;swap(Nu,myGrid.numberOfActiveUDisplacementFV);
	int Nv;swap(Nv,myGrid.numberOfActiveVDisplacementFV);
	int NP;swap(NP,myGrid.numberOfActiveGeneralFV);
	double dx;swap(dx,myGrid.dx);
	double dy;swap(dy,myGrid.dy);
	double dt;swap(dt,myGrid.dt);
	double h;swap(h,myGrid.h);
	vector<vector<int>> idU;swap(idU,myGrid.uDisplacementFVIndex);
	vector<vector<int>> idV;swap(idV,myGrid.vDisplacementFVIndex);
	vector<vector<int>> idP;swap(idP,myGrid.generalFVIndex);
	vector<vector<int>> cooU;swap(cooU,myGrid.uDisplacementFVCoordinates);
	vector<vector<int>> cooV;swap(cooV,myGrid.vDisplacementFVCoordinates);
	vector<vector<int>> cooP;swap(cooP,myGrid.generalFVCoordinates);
	vector<vector<int>> horFaceStatus;swap(horFaceStatus,myGrid.horizontalFacesStatus);
	vector<vector<int>> verFaceStatus;swap(verFaceStatus,myGrid.verticalFacesStatus);
	vector<vector<double>> uField;swap(uField,myGrid.uDisplacementField);
	vector<vector<double>> vField;swap(vField,myGrid.vDisplacementField);
	vector<vector<double>> pField;swap(pField,myGrid.pressureField);

/*		PROBLEM PARAMETERS CALCULATION
	----------------------------------------------------------------*/

	// Constructor
	problemParameters myProblem(dx,dy,K,phi,rho_s,c_s,mu_f,rho_f,c_f,G,lambda,sigmab,Lx,Ly,
		uField,vField,pField,cooU,cooV,cooP,idU,idV,idP,g);

	// Apply initial conditions
	myProblem.applySealedColumnInitialConditions();

	// Passing variables
	double Q;swap(Q,myProblem.Q);
	double alpha;swap(alpha,myProblem.alpha);
	double storageCoefficient=1/Q;
	double longitudinalModulus;swap(longitudinalModulus,myProblem.M);
	double consolidationCoefficient;swap(consolidationCoefficient,myProblem.c);
	double minimumTimeStepVerruijt;swap(minimumTimeStepVerruijt,myProblem.dt_vv);
	double dt_carlos;swap(dt_carlos,myProblem.dt_carlos);
	double rho=(phi*rho_f+(1-phi)*rho_s);
	double initialPressure;swap(initialPressure,myProblem.P0);
	uField=myProblem.uDisplacementField;
	vField=myProblem.vDisplacementField;
	pField=myProblem.pressureField;
	
/*		LINEAR SYSTEM'S COEFFICIENTS MATRIX ASSEMBLY
	----------------------------------------------------------------*/

	// Constructor
	coefficientsAssembly myCoefficients(bcType,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);

	// Coefficients matrix assembly
	myCoefficients.assemblyCoefficientsMatrix(dx,dy,dt,G,lambda,alpha,K,mu_f,Q,rho,g);

	// Passing variables
	vector<vector<double>> coefficientsMatrix;swap(coefficientsMatrix,
		myCoefficients.coefficientsMatrix);
	vector<double> sparseCoefficientsRow;swap(sparseCoefficientsRow,
		myCoefficients.sparseCoefficientsRow);
	vector<double> sparseCoefficientsColumn;swap(sparseCoefficientsColumn,
		myCoefficients.sparseCoefficientsColumn);
	vector<double> sparseCoefficientsValue;swap(sparseCoefficientsValue,
		myCoefficients.sparseCoefficientsValue);

/*		LINEAR SYSTEM SOLVER
	----------------------------------------------------------------*/

	// Variables declaration
	int timeStep;
	vector<double> independentTermsArray;

	// Constructors
	independentTermsAssembly myIndependentTerms(bcType,bcValue,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);
	linearSystemSolver myLinearSystemSolver(coefficientsMatrix,sparseCoefficientsRow,
		sparseCoefficientsColumn,sparseCoefficientsValue,uField,vField,pField,Nu,Nv,NP,Nt,idU,idV,
		idP,cooU,cooV,cooP);

	// LU Factorization of coefficientsMatrix
	ierr=myLinearSystemSolver.coefficientsMatrixLUFactorization();CHKERRQ(ierr);
	
	// Creation of arrays
	ierr=myLinearSystemSolver.createPETScArrays();CHKERRQ(ierr);
	ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

	for(timeStep=0; timeStep<Nt-1; timeStep++)
	{
		// Assembly of the independent terms array
		myIndependentTerms.assemblyIndependentTermsArray(dx,dy,dt,G,lambda,alpha,K,mu_f,Q,rho,g,
			uField,vField,pField,timeStep);

		// Passing independent terms array
		independentTermsArray=myIndependentTerms.independentTermsArray;

		// Solution of the linear system
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setRHSValue(independentTermsArray);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.solveLinearSystem();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setFieldValue(timeStep+1);CHKERRQ(ierr);

		// Passing solutions
		uField=myLinearSystemSolver.uField;
		vField=myLinearSystemSolver.vField;
		pField=myLinearSystemSolver.pField;
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

		cout << timeStep+1<< "\r";
	}

	cout << Ny << "x" << Nx << "x" << Nt-1 << " ";
	cout << "(h=" << h << ", dt=" << dt << ")\n";

/*		DATA PROCESSING
	----------------------------------------------------------------*/
	
	// Variables declaration
	vector<int> exportedTimeSteps=
	{
		{1},
		{(Nt-1)/8},
		{(Nt-1)/2},
		{Nt-1}
	};

	// Constructor
	dataProcessing myDataProcessing(idU,idV,idP,uField,vField,pField,gridType,interpScheme,dx,dy);

	// Exports data for specified time-steps
	for(int i=0; i<exportedTimeSteps.size(); i++)
	{
		myDataProcessing.exportSealedColumnAnalyticalSolution(Ly,alpha,Q,rho,g,rho_f,
			longitudinalModulus,sigmab,dt,exportedTimeSteps[i],consolidationCoefficient,pairName);
		myDataProcessing.exportSealedColumnNumericalSolution(dy,dt,Ly,exportedTimeSteps[i],
			pairName);
	}

	return ierr;
};

int terzaghi(string gridType, string interpScheme, int Nt, int meshSize, double Lt, double g,
	double sigmab, poroelasticProperties myProperties)
{
	PetscErrorCode ierr;

/*		ENTRY PARAMETERS
	----------------------------------------------------------------*/

	// Grid parameters
	int Nx=meshSize;
	int Ny=6*meshSize;

	// Reservoir parameters
	double Lx=1; // [m]
	double Ly=6; // [m]

	vector<vector<double>> sCoordinates=
	{
		{Lx,Ly},
		{0,Ly},
		{0,0},
		{Lx,0}
	};

	// Bulk properties
	string pairName=myProperties.pairName;
	double G=myProperties.shearModulus;
	double lambda=myProperties.bulkModulus-2*G/3;
	double phi=myProperties.porosity;
	double K=myProperties.permeability;

	// Solid properties
	double c_s=1/myProperties.solidBulkModulus;
	double rho_s=myProperties.solidDensity;

	// Fluid properties
	double c_f=1/myProperties.fluidBulkModulus;
	double rho_f=myProperties.fluidDensity;
	double mu_f=myProperties.fluidViscosity;

	// BC types ({u,v,P} 1 for Dirichlet and 0 for Neumann, -1 for Stress/Fluid Flow, starts on
	// "north" and follows counterclockwise)
	vector<vector<int>> bcType=
	{
		{-1,-1,1},
		{1,-1,-1},
		{-1,1,0},
		{1,-1,-1}
	};

	// BC values ({u,v,P}, starts on "north" and follows counterclockwise)
	vector<vector<double>> bcValue=
	{
		{0,sigmab,0},
		{0,0,0},
		{0,0,rho_f*g},
		{0,0,0}
	};

/*		GRID CREATION
	----------------------------------------------------------------*/

	// Constructor
	gridDesign myGrid(Nx,Ny,Nt,Lx,Ly,Lt,gridType,sCoordinates);

	// Passing variables
	int Nu;swap(Nu,myGrid.numberOfActiveUDisplacementFV);
	int Nv;swap(Nv,myGrid.numberOfActiveVDisplacementFV);
	int NP;swap(NP,myGrid.numberOfActiveGeneralFV);
	double dx;swap(dx,myGrid.dx);
	double dy;swap(dy,myGrid.dy);
	double dt;swap(dt,myGrid.dt);
	double h;swap(h,myGrid.h);
	vector<vector<int>> idU;swap(idU,myGrid.uDisplacementFVIndex);
	vector<vector<int>> idV;swap(idV,myGrid.vDisplacementFVIndex);
	vector<vector<int>> idP;swap(idP,myGrid.generalFVIndex);
	vector<vector<int>> cooU;swap(cooU,myGrid.uDisplacementFVCoordinates);
	vector<vector<int>> cooV;swap(cooV,myGrid.vDisplacementFVCoordinates);
	vector<vector<int>> cooP;swap(cooP,myGrid.generalFVCoordinates);
	vector<vector<int>> horFaceStatus;swap(horFaceStatus,myGrid.horizontalFacesStatus);
	vector<vector<int>> verFaceStatus;swap(verFaceStatus,myGrid.verticalFacesStatus);
	vector<vector<double>> uField;swap(uField,myGrid.uDisplacementField);
	vector<vector<double>> vField;swap(vField,myGrid.vDisplacementField);
	vector<vector<double>> pField;swap(pField,myGrid.pressureField);

/*		PROBLEM PARAMETERS CALCULATION
	----------------------------------------------------------------*/

	// Constructor
	problemParameters myProblem(dx,dy,K,phi,rho_s,c_s,mu_f,rho_f,c_f,G,lambda,sigmab,Lx,Ly,
		uField,vField,pField,cooU,cooV,cooP,idU,idV,idP,g);

	// Apply initial conditions
	myProblem.applyTerzaghiInitialConditions();

	// Passing variables
	double Q;swap(Q,myProblem.Q);
	double alpha;swap(alpha,myProblem.alpha);
	double storageCoefficient=1/Q;
	double longitudinalModulus;swap(longitudinalModulus,myProblem.M);
	double consolidationCoefficient;swap(consolidationCoefficient,myProblem.c);
	double minimumTimeStepVerruijt;swap(minimumTimeStepVerruijt,myProblem.dt_vv);
	double dt_carlos;swap(dt_carlos,myProblem.dt_carlos);
	double rho=(phi*rho_f+(1-phi)*rho_s);
	double initialPressure;swap(initialPressure,myProblem.P0);
	uField=myProblem.uDisplacementField;
	vField=myProblem.vDisplacementField;
	pField=myProblem.pressureField;
	
/*		LINEAR SYSTEM'S COEFFICIENTS MATRIX ASSEMBLY
	----------------------------------------------------------------*/

	// Constructor
	coefficientsAssembly myCoefficients(bcType,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);

	// Coefficients matrix assembly
	myCoefficients.assemblyCoefficientsMatrix(dx,dy,dt,G,lambda,alpha,K,mu_f,Q,rho,g);

	// Passing variables
	vector<vector<double>> coefficientsMatrix;swap(coefficientsMatrix,
		myCoefficients.coefficientsMatrix);
	vector<double> sparseCoefficientsRow;swap(sparseCoefficientsRow,
		myCoefficients.sparseCoefficientsRow);
	vector<double> sparseCoefficientsColumn;swap(sparseCoefficientsColumn,
		myCoefficients.sparseCoefficientsColumn);
	vector<double> sparseCoefficientsValue;swap(sparseCoefficientsValue,
		myCoefficients.sparseCoefficientsValue);

/*		LINEAR SYSTEM SOLVER
	----------------------------------------------------------------*/

	// Variables declaration
	int timeStep;
	vector<double> independentTermsArray;

	// Constructors
	independentTermsAssembly myIndependentTerms(bcType,bcValue,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);
	linearSystemSolver myLinearSystemSolver(coefficientsMatrix,sparseCoefficientsRow,
		sparseCoefficientsColumn,sparseCoefficientsValue,uField,vField,pField,Nu,Nv,NP,Nt,idU,idV,
		idP,cooU,cooV,cooP);

	// LU Factorization of coefficientsMatrix
	ierr=myLinearSystemSolver.coefficientsMatrixLUFactorization();CHKERRQ(ierr);
	
	// Creation of arrays
	ierr=myLinearSystemSolver.createPETScArrays();CHKERRQ(ierr);
	ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

	for(timeStep=0; timeStep<Nt-1; timeStep++)
	{
		// Assembly of the independent terms array
		myIndependentTerms.assemblyIndependentTermsArray(dx,dy,dt,G,lambda,alpha,K,mu_f,Q,rho,g,
			uField,vField,pField,timeStep);

		// Passing independent terms array
		independentTermsArray=myIndependentTerms.independentTermsArray;

		// Solution of the linear system
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setRHSValue(independentTermsArray);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.solveLinearSystem();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setFieldValue(timeStep+1);CHKERRQ(ierr);

		// Passing solutions
		uField=myLinearSystemSolver.uField;
		vField=myLinearSystemSolver.vField;
		pField=myLinearSystemSolver.pField;
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

		cout << timeStep+1<< "\r";
	}

	cout << Ny << "x" << Nx << "x" << Nt-1 << " ";
	cout << "(h=" << h << ", dt=" << dt << ")\n";

/*		DATA PROCESSING
	----------------------------------------------------------------*/
	
	// Variables declaration
	vector<int> exportedTimeSteps=
	{
		{1},
		{(Nt-1)/8},
		{(Nt-1)/2},
		{Nt-1}
	};
	if(Nt==2)
	{
		exportedTimeSteps.clear();
		exportedTimeSteps.push_back(1);
	}

	// Constructor
	dataProcessing myDataProcessing(idU,idV,idP,uField,vField,pField,gridType,interpScheme,dx,dy);

	// Exports data for specified time-steps
	for(int i=0; i<exportedTimeSteps.size(); i++)
	{
		myDataProcessing.exportTerzaghiAnalyticalSolution(Ly,alpha,Q,rho,g,rho_f,
			longitudinalModulus,sigmab,dt,exportedTimeSteps[i],consolidationCoefficient,pairName);
		myDataProcessing.exportTerzaghiNumericalSolution(dy,dt,Ly,exportedTimeSteps[i],pairName);
	}

	return ierr;
};

int mandel(string gridType, string interpScheme, int Nt, int meshSize, double Lt, double g,
	double forceb, poroelasticProperties myProperties)
{
	PetscErrorCode ierr;

/*		ENTRY PARAMETERS
	----------------------------------------------------------------*/

	// Grid parameters
	int Nx=5*meshSize;
	int Ny=5*meshSize;

	// Reservoir parameters
	double Lx=5; // [m]
	double Ly=5; // [m]
	vector<vector<double>> sCoordinates=
	{
		{Lx,Ly},
		{0,Ly},
		{0,0},
		{Lx,0}
	};

	// Bulk properties
	string pairName=myProperties.pairName;
	double G=myProperties.shearModulus;
	double lambda=myProperties.bulkModulus-2*G/3;
	double phi=myProperties.porosity;
	double K=myProperties.permeability;

	// Solid properties
	double c_s=1/myProperties.solidBulkModulus;
	double rho_s=myProperties.solidDensity;

	// Fluid properties
	double c_f=1/myProperties.fluidBulkModulus;
	double rho_f=myProperties.fluidDensity;
	double mu_f=myProperties.fluidViscosity;

	// BC types ({u,v,P} 1 for Dirichlet and 0 for Neumann, -1 for Stress/Fluid Flow, starts on
	// "north" and follows counterclockwise)
	vector<vector<int>> bcType=
	{
		{-1,-1,0},
		{1,-1,-1},
		{-1,1,0},
		{-1,-1,1}
	};

	// BC values ({u,v,P}, starts on "north" and follows counterclockwise)
	vector<vector<double>> bcValue=
	{
		{0,0,rho_f*g},
		{0,0,0},
		{0,0,rho_f*g},
		{0,0,0}
	};

/*		GRID CREATION
	----------------------------------------------------------------*/

	// Constructor
	gridDesign myGrid(Nx,Ny,Nt,Lx,Ly,Lt,gridType,sCoordinates);

	// Passing variables
	int Nu;swap(Nu,myGrid.numberOfActiveUDisplacementFV);
	int Nv;swap(Nv,myGrid.numberOfActiveVDisplacementFV);
	int NP;swap(NP,myGrid.numberOfActiveGeneralFV);
	double dx;swap(dx,myGrid.dx);
	double dy;swap(dy,myGrid.dy);
	double dt;swap(dt,myGrid.dt);
	double h;swap(h,myGrid.h);
	vector<vector<int>> idU;swap(idU,myGrid.uDisplacementFVIndex);
	vector<vector<int>> idV;swap(idV,myGrid.vDisplacementFVIndex);
	vector<vector<int>> idP;swap(idP,myGrid.generalFVIndex);
	vector<vector<int>> cooU;swap(cooU,myGrid.uDisplacementFVCoordinates);
	vector<vector<int>> cooV;swap(cooV,myGrid.vDisplacementFVCoordinates);
	vector<vector<int>> cooP;swap(cooP,myGrid.generalFVCoordinates);
	vector<vector<int>> horFaceStatus;swap(horFaceStatus,myGrid.horizontalFacesStatus);
	vector<vector<int>> verFaceStatus;swap(verFaceStatus,myGrid.verticalFacesStatus);
	vector<vector<double>> uField;swap(uField,myGrid.uDisplacementField);
	vector<vector<double>> vField;swap(vField,myGrid.vDisplacementField);
	vector<vector<double>> pField;swap(pField,myGrid.pressureField);

/*		PROBLEM PARAMETERS CALCULATION
	----------------------------------------------------------------*/

	// Constructor
	problemParameters myProblem(dx,dy,K,phi,rho_s,c_s,mu_f,rho_f,c_f,G,lambda,forceb,Lx,Ly,
		uField,vField,pField,cooU,cooV,cooP,idU,idV,idP,g);

	// Apply initial conditions
	myProblem.applyMandelInitialConditions();

	// Passing variables
	double Q;swap(Q,myProblem.Q);
	double alpha;swap(alpha,myProblem.alpha);
	double storageCoefficient=1/Q;
	double longitudinalModulus;swap(longitudinalModulus,myProblem.M);
	double consolidationCoefficient;swap(consolidationCoefficient,myProblem.c);
	double minimumTimeStepVerruijt;swap(minimumTimeStepVerruijt,myProblem.dt_vv);
	double dt_carlos;swap(dt_carlos,myProblem.dt_carlos);
	double rho=(phi*rho_f+(1-phi)*rho_s);
	double initialPressure;swap(initialPressure,myProblem.P0);
	uField=myProblem.uDisplacementField;
	vField=myProblem.vDisplacementField;
	pField=myProblem.pressureField;
	
/*		LINEAR SYSTEM'S COEFFICIENTS MATRIX ASSEMBLY
	----------------------------------------------------------------*/

	// Constructor
	coefficientsAssembly myCoefficients(bcType,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);

	// Coefficients matrix assembly
	myCoefficients.assemblyCoefficientsMatrix(dx,dy,dt,G,lambda,alpha,K,mu_f,Q,rho,g);
	myCoefficients.assemblyMandelCoefficientsMatrix(dx,dy,G,lambda,alpha);
	myCoefficients.assemblySparseMatrix(myCoefficients.coefficientsMatrix);

	// Passing variables
	vector<vector<double>> coefficientsMatrix;swap(coefficientsMatrix,
		myCoefficients.coefficientsMatrix);
	vector<double> sparseCoefficientsRow;swap(sparseCoefficientsRow,
		myCoefficients.sparseCoefficientsRow);
	vector<double> sparseCoefficientsColumn;swap(sparseCoefficientsColumn,
		myCoefficients.sparseCoefficientsColumn);
	vector<double> sparseCoefficientsValue;swap(sparseCoefficientsValue,
		myCoefficients.sparseCoefficientsValue);

/*		LINEAR SYSTEM SOLVER
	----------------------------------------------------------------*/

	// Variables declaration
	int timeStep;
	vector<double> independentTermsArray;

	// Constructors
	independentTermsAssembly myIndependentTerms(bcType,bcValue,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);
	linearSystemSolver myLinearSystemSolver(coefficientsMatrix,sparseCoefficientsRow,
		sparseCoefficientsColumn,sparseCoefficientsValue,uField,vField,pField,Nu,Nv,NP,Nt,idU,idV,
		idP,cooU,cooV,cooP);

	// Increase the independent terms array
	myIndependentTerms.increaseMandelIndependentTermsArray();

	// LU Factorization of coefficientsMatrix
	ierr=myLinearSystemSolver.coefficientsMatrixLUFactorization();CHKERRQ(ierr);

	// Creation of arrays
	ierr=myLinearSystemSolver.createPETScArrays();CHKERRQ(ierr);
	ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

	for(timeStep=0; timeStep<Nt-1; timeStep++)
	{
		// Assembly of the independent terms array
		myIndependentTerms.assemblyIndependentTermsArray(dx,dy,dt,G,lambda,alpha,K,mu_f,Q,rho,g,
			uField,vField,pField,timeStep);
		myIndependentTerms.assemblyMandelIndependentTermsArray(forceb,Lx);

		// Passing independent terms array
		independentTermsArray=myIndependentTerms.independentTermsArray;
		
		// Solution of the linear system
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setRHSValue(independentTermsArray);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.solveLinearSystem();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setFieldValue(timeStep+1);CHKERRQ(ierr);

		// Passing solutions
		uField=myLinearSystemSolver.uField;
		vField=myLinearSystemSolver.vField;
		pField=myLinearSystemSolver.pField;
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

		cout << timeStep+1<< "\r";
	}

	cout << Ny << "x" << Nx << "x" << Nt-1 << " ";
	cout << "(h=" << h << ", dt=" << dt << ")\n";

/*		DATA PROCESSING
	----------------------------------------------------------------*/
	
	// Variables declaration
	vector<int> exportedTimeSteps=
	{
		{1},
		{(Nt-1)/16},
		{(Nt-1)/4},
		{(Nt-1)}
	};
	if(Nt==2)
	{
		exportedTimeSteps.clear();
		exportedTimeSteps.push_back(1);
	}

	// Constructor
	dataProcessing myDataProcessing(idU,idV,idP,uField,vField,pField,gridType,interpScheme,dx,dy);

	// Gets Mandel transcendental equation roots
	myDataProcessing.findMandelRoots(initialPressure,forceb,Lx,alpha,longitudinalModulus,lambda,Q);

	// Exports data for specified time-steps
	for(int i=0; i<exportedTimeSteps.size(); i++)
	{
		myDataProcessing.exportMandelAnalyticalSolution(Lx,Ly,consolidationCoefficient,
			initialPressure,alpha,Q,longitudinalModulus,lambda,forceb,K,mu_f,dt,
			exportedTimeSteps[i],pairName);
		myDataProcessing.exportMandelNumericalSolution(dx,dy,dt,Lx,Ly,exportedTimeSteps[i],
			pairName);
	}
	
	return ierr;
};

int convergence(string gridType, string interpScheme, int Nt, int meshSize, double Lt, double g,
	double sigmab, poroelasticProperties myProperties)
{
	PetscErrorCode ierr;

/*		ENTRY PARAMETERS
	----------------------------------------------------------------*/

	// Grid parameters
	int Nx=meshSize;
	int Ny=6*meshSize;

	// Reservoir parameters
	double Lx=1; // [m]
	double Ly=6; // [m]

	vector<vector<double>> sCoordinates=
	{
		{Lx,Ly},
		{0,Ly},
		{0,0},
		{Lx,0}
	};

	// Bulk properties
	string pairName=myProperties.pairName;
	double G=myProperties.shearModulus;
	double lambda=myProperties.bulkModulus-2*G/3;
	double phi=myProperties.porosity;
	double K=myProperties.permeability;

	// Solid properties
	double c_s=1/myProperties.solidBulkModulus;
	double rho_s=myProperties.solidDensity;

	// Fluid properties
	double c_f=1/myProperties.fluidBulkModulus;
	double rho_f=myProperties.fluidDensity;
	double mu_f=myProperties.fluidViscosity;

	// BC types ({u,v,P} 1 for Dirichlet and 0 for Neumann, -1 for Stress/Fluid Flow, starts on
	// "north" and follows counterclockwise)
	vector<vector<int>> bcType=
	{
		{-1,-1,1},
		{1,-1,-1},
		{-1,1,0},
		{1,-1,-1}
	};

	// BC values ({u,v,P}, starts on "north" and follows counterclockwise)
	vector<vector<double>> bcValue=
	{
		{0,sigmab,0},
		{0,0,0},
		{0,0,rho_f*g},
		{0,0,0}
	};

/*		GRID CREATION
	----------------------------------------------------------------*/

	// Constructor
	gridDesign myGrid(Nx,Ny,Nt,Lx,Ly,Lt,gridType,sCoordinates);

	// Passing variables
	int Nu;swap(Nu,myGrid.numberOfActiveUDisplacementFV);
	int Nv;swap(Nv,myGrid.numberOfActiveVDisplacementFV);
	int NP;swap(NP,myGrid.numberOfActiveGeneralFV);
	double dx;swap(dx,myGrid.dx);
	double dy;swap(dy,myGrid.dy);
	double dt;swap(dt,myGrid.dt);
	double h;swap(h,myGrid.h);
	vector<vector<int>> idU;swap(idU,myGrid.uDisplacementFVIndex);
	vector<vector<int>> idV;swap(idV,myGrid.vDisplacementFVIndex);
	vector<vector<int>> idP;swap(idP,myGrid.generalFVIndex);
	vector<vector<int>> cooU;swap(cooU,myGrid.uDisplacementFVCoordinates);
	vector<vector<int>> cooV;swap(cooV,myGrid.vDisplacementFVCoordinates);
	vector<vector<int>> cooP;swap(cooP,myGrid.generalFVCoordinates);
	vector<vector<int>> horFaceStatus;swap(horFaceStatus,myGrid.horizontalFacesStatus);
	vector<vector<int>> verFaceStatus;swap(verFaceStatus,myGrid.verticalFacesStatus);
	vector<vector<double>> uField;swap(uField,myGrid.uDisplacementField);
	vector<vector<double>> vField;swap(vField,myGrid.vDisplacementField);
	vector<vector<double>> pField;swap(pField,myGrid.pressureField);

/*		PROBLEM PARAMETERS CALCULATION
	----------------------------------------------------------------*/

	// Constructor
	problemParameters myProblem(dx,dy,K,phi,rho_s,c_s,mu_f,rho_f,c_f,G,lambda,sigmab,Lx,Ly,
		uField,vField,pField,cooU,cooV,cooP,idU,idV,idP,g);

	// Apply initial conditions
	myProblem.applyTerzaghiInitialConditions();

	// Passing variables
	double Q;swap(Q,myProblem.Q);
	double alpha;swap(alpha,myProblem.alpha);
	double storageCoefficient=1/Q;
	double longitudinalModulus;swap(longitudinalModulus,myProblem.M);
	double consolidationCoefficient;swap(consolidationCoefficient,myProblem.c);
	double minimumTimeStepVerruijt;swap(minimumTimeStepVerruijt,myProblem.dt_vv);
	double dt_carlos;swap(dt_carlos,myProblem.dt_carlos);
	double rho=(phi*rho_f+(1-phi)*rho_s);
	double initialPressure;swap(initialPressure,myProblem.P0);
	uField=myProblem.uDisplacementField;
	vField=myProblem.vDisplacementField;
	pField=myProblem.pressureField;
	
/*		LINEAR SYSTEM'S COEFFICIENTS MATRIX ASSEMBLY
	----------------------------------------------------------------*/

	// Constructor
	coefficientsAssembly myCoefficients(bcType,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);

	// Coefficients matrix assembly
	myCoefficients.assemblyCoefficientsMatrix(dx,dy,dt,G,lambda,alpha,K,mu_f,Q,rho,g);

	// Passing variables
	vector<vector<double>> coefficientsMatrix;swap(coefficientsMatrix,
		myCoefficients.coefficientsMatrix);
	vector<double> sparseCoefficientsRow;swap(sparseCoefficientsRow,
		myCoefficients.sparseCoefficientsRow);
	vector<double> sparseCoefficientsColumn;swap(sparseCoefficientsColumn,
		myCoefficients.sparseCoefficientsColumn);
	vector<double> sparseCoefficientsValue;swap(sparseCoefficientsValue,
		myCoefficients.sparseCoefficientsValue);

/*		LINEAR SYSTEM SOLVER
	----------------------------------------------------------------*/

	// Variables declaration
	int timeStep;
	vector<double> independentTermsArray;

	// Constructors
	independentTermsAssembly myIndependentTerms(bcType,bcValue,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);
	linearSystemSolver myLinearSystemSolver(coefficientsMatrix,sparseCoefficientsRow,
		sparseCoefficientsColumn,sparseCoefficientsValue,uField,vField,pField,Nu,Nv,NP,Nt,idU,idV,
		idP,cooU,cooV,cooP);

	// LU Factorization of coefficientsMatrix
	ierr=myLinearSystemSolver.coefficientsMatrixLUFactorization();CHKERRQ(ierr);
	
	// Creation of arrays
	ierr=myLinearSystemSolver.createPETScArrays();CHKERRQ(ierr);
	ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

	for(timeStep=0; timeStep<Nt-1; timeStep++)
	{
		// Assembly of the independent terms array
		myIndependentTerms.assemblyIndependentTermsArray(dx,dy,dt,G,lambda,alpha,K,mu_f,Q,rho,g,
			uField,vField,pField,timeStep);

		// Passing independent terms array
		independentTermsArray=myIndependentTerms.independentTermsArray;

		// Solution of the linear system
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setRHSValue(independentTermsArray);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.solveLinearSystem();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setFieldValue(timeStep+1);CHKERRQ(ierr);

		// Passing solutions
		uField=myLinearSystemSolver.uField;
		vField=myLinearSystemSolver.vField;
		pField=myLinearSystemSolver.pField;
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

		cout << timeStep+1<< "\r";
	}

	cout << Ny << "x" << Nx << "x" << Nt-1 << " ";
	cout << "(h=" << h << ", dt=" << dt;

/*		DATA PROCESSING
	----------------------------------------------------------------*/
	
	// Constructor
	dataProcessing myDataProcessing(idU,idV,idP,uField,vField,pField,gridType,interpScheme,dx,dy);

	// Gets error norm
	myDataProcessing.getTerzaghiErrorNorm(dy,dt,h,Ly,initialPressure,consolidationCoefficient,
		alpha,longitudinalModulus,sigmab,Q,rho,g,rho_f);
	double pErrorNorm=myDataProcessing.myErrorNorm.p;
	double vErrorNorm=myDataProcessing.myErrorNorm.v;
	cout << ", pErrorNorm=" << pErrorNorm << ", vErrorNorm=" << vErrorNorm << ")\n";

	return ierr;
};

int stripfoot(string gridType, string interpScheme, int Nt, int meshSize, double Lt, double g,
	double sigmab, poroelasticProperties myProperties)
{
	PetscErrorCode ierr;

/*		ENTRY PARAMETERS
	----------------------------------------------------------------*/

	// Grid parameters
	int Nx=5*meshSize;
	int Ny=5*meshSize;
	int stripSize=meshSize;

	// Reservoir parameters
	double Lx=5; // [m]
	double Ly=5; // [m]

	vector<vector<double>> sCoordinates=
	{
		{Lx,Ly},
		{0,Ly},
		{0,0},
		{Lx,0}
	};

	// Bulk properties
	string pairName=myProperties.pairName;
	double G=myProperties.shearModulus;
	double lambda=myProperties.bulkModulus-2*G/3;
	double phi=myProperties.porosity;
	double K=myProperties.permeability;

	// Solid properties
	double c_s=1/myProperties.solidBulkModulus;
	double rho_s=myProperties.solidDensity;

	// Fluid properties
	double c_f=1/myProperties.fluidBulkModulus;
	double rho_f=myProperties.fluidDensity;
	double mu_f=myProperties.fluidViscosity;

	// BC types ({u,v,p-micro,p-macro} 1 for Dirichlet and 0 for Neumann, -1 for Stress/Fluid Flow, starts on
	// "north" and follows counterclockwise)
	vector<vector<int>> bcType=
	{
		{-1,-1,-1},
		{1,-1,-1},
		{-1,1,-1},
		{1,-1,-1}
	};

	// BC values ({u,v,P}, starts on "north" and follows counterclockwise)
	vector<vector<double>> bcValue=
	{
		{0,0,0},
		{0,0,0},
		{0,0,0},
		{0,0,0}
	};

/*		GRID CREATION
	----------------------------------------------------------------*/

	// Constructor
	gridDesign myGrid(Nx,Ny,Nt,Lx,Ly,Lt,gridType,sCoordinates);

	// Passing variables
	int Nu;swap(Nu,myGrid.numberOfActiveUDisplacementFV);
	int Nv;swap(Nv,myGrid.numberOfActiveVDisplacementFV);
	int NP;swap(NP,myGrid.numberOfActiveGeneralFV);
	double dx;swap(dx,myGrid.dx);
	double dy;swap(dy,myGrid.dy);
	double dt;swap(dt,myGrid.dt);
	double h;swap(h,myGrid.h);
	vector<vector<int>> idU;swap(idU,myGrid.uDisplacementFVIndex);
	vector<vector<int>> idV;swap(idV,myGrid.vDisplacementFVIndex);
	vector<vector<int>> idP;swap(idP,myGrid.generalFVIndex);
	vector<vector<int>> cooU;swap(cooU,myGrid.uDisplacementFVCoordinates);
	vector<vector<int>> cooV;swap(cooV,myGrid.vDisplacementFVCoordinates);
	vector<vector<int>> cooP;swap(cooP,myGrid.generalFVCoordinates);
	vector<vector<int>> horFaceStatus;swap(horFaceStatus,myGrid.horizontalFacesStatus);
	vector<vector<int>> verFaceStatus;swap(verFaceStatus,myGrid.verticalFacesStatus);
	vector<vector<double>> uField;swap(uField,myGrid.uDisplacementField);
	vector<vector<double>> vField;swap(vField,myGrid.vDisplacementField);
	vector<vector<double>> pField;swap(pField,myGrid.pressureField);

/*		PROBLEM PARAMETERS CALCULATION
	----------------------------------------------------------------*/

	// Constructor
	problemParameters myProblem(dx,dy,K,phi,rho_s,c_s,mu_f,rho_f,c_f,G,lambda,sigmab,Lx,Ly,
		uField,vField,pField,cooU,cooV,cooP,idU,idV,idP,g);

	// Apply initial conditions
	myProblem.applyTerzaghiInitialConditions();

	// Passing variables
	double Q;swap(Q,myProblem.Q);
	double alpha;swap(alpha,myProblem.alpha);
	double storageCoefficient=1/Q;
	double longitudinalModulus;swap(longitudinalModulus,myProblem.M);
	double consolidationCoefficient;swap(consolidationCoefficient,myProblem.c);
	double minimumTimeStepVerruijt;swap(minimumTimeStepVerruijt,myProblem.dt_vv);
	double dt_carlos;swap(dt_carlos,myProblem.dt_carlos);
	double rho=(phi*rho_f+(1-phi)*rho_s);
	double initialPressure;swap(initialPressure,myProblem.P0);
	uField=myProblem.uDisplacementField;
	vField=myProblem.vDisplacementField;
	pField=myProblem.pressureField;
	
/*		LINEAR SYSTEM'S COEFFICIENTS MATRIX ASSEMBLY
	----------------------------------------------------------------*/

	// Constructor
	coefficientsAssembly myCoefficients(bcType,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);

	// Coefficients matrix assembly
	myCoefficients.assemblyCoefficientsMatrix(dx,dy,dt,G,lambda,alpha,K,mu_f,Q,rho,g);
	myCoefficients.addStripfootBC(stripSize,K,mu_f);

	// Passing variables
	vector<vector<double>> coefficientsMatrix;swap(coefficientsMatrix,
		myCoefficients.coefficientsMatrix);
	vector<double> sparseCoefficientsRow;swap(sparseCoefficientsRow,
		myCoefficients.sparseCoefficientsRow);
	vector<double> sparseCoefficientsColumn;swap(sparseCoefficientsColumn,
		myCoefficients.sparseCoefficientsColumn);
	vector<double> sparseCoefficientsValue;swap(sparseCoefficientsValue,
		myCoefficients.sparseCoefficientsValue);

/*		LINEAR SYSTEM SOLVER
	----------------------------------------------------------------*/

	// Variables declaration
	int timeStep;
	vector<double> independentTermsArray;

	// Constructors
	independentTermsAssembly myIndependentTerms(bcType,bcValue,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);
	linearSystemSolver myLinearSystemSolver(coefficientsMatrix,sparseCoefficientsRow,
		sparseCoefficientsColumn,sparseCoefficientsValue,uField,vField,pField,Nu,Nv,NP,Nt,idU,idV,
		idP,cooU,cooV,cooP);

	// LU Factorization of coefficientsMatrix
	ierr=myLinearSystemSolver.coefficientsMatrixLUFactorization();CHKERRQ(ierr);
	
	// Creation of arrays
	ierr=myLinearSystemSolver.createPETScArrays();CHKERRQ(ierr);
	ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

	for(timeStep=0; timeStep<Nt-1; timeStep++)
	{
		// Assembly of the independent terms array
		myIndependentTerms.assemblyIndependentTermsArray(dx,dy,dt,G,lambda,alpha,K,mu_f,Q,rho,g,
			uField,vField,pField,timeStep);
		myIndependentTerms.addStripfootBC(stripSize,dx,sigmab);

		// Passing independent terms array
		independentTermsArray=myIndependentTerms.independentTermsArray;

		// Solution of the linear system
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setRHSValue(independentTermsArray);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.solveLinearSystem();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setFieldValue(timeStep+1);CHKERRQ(ierr);

		// Passing solutions
		uField=myLinearSystemSolver.uField;
		vField=myLinearSystemSolver.vField;
		pField=myLinearSystemSolver.pField;
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

		cout << timeStep+1<< "\r";
	}

	cout << Ny << "x" << Nx << "x" << Nt-1 << " ";
	cout << "(h=" << h << ", dt=" << dt << ")\n";

/*		DATA PROCESSING
	----------------------------------------------------------------*/
	
	// Variables declaration
	vector<int> exportedTimeSteps=
	{
		{1},
		{(Nt-1)/8},
		{(Nt-1)/2},
		{Nt-1}
	};
	if(Nt==2)
	{
		exportedTimeSteps.clear();
		exportedTimeSteps.push_back(1);
	}

	// Constructor
	dataProcessing myDataProcessing(idU,idV,idP,uField,vField,pField,gridType,interpScheme,dx,dy);

	// Exports data for specified time-steps
	for(int i=0; i<exportedTimeSteps.size(); i++)
	{
		myDataProcessing.exportStripfootTSolution(dx,dy,dt,Ly,exportedTimeSteps[i],pairName);
		myDataProcessing.exportStripfootHSolution(dx,dy,h,Ly,exportedTimeSteps[i],pairName);
	}
	
	return ierr;
};

int terzaghiDouble(string gridType, string interpScheme, int Nt, int meshSize, double Lt, double g,
	double sigmab, poroelasticProperties myProperties)
{
	PetscErrorCode ierr;

/*		ENTRY PARAMETERS
	----------------------------------------------------------------*/

	// Grid parameters
	int Nx=meshSize;
	int Ny=6*meshSize;

	// Reservoir parameters
	double Lx=1; // [m]
	double Ly=6; // [m]

	vector<vector<double>> sCoordinates=
	{
		{Lx,Ly},
		{0,Ly},
		{0,0},
		{Lx,0}
	};

	// Bulk properties
	string pairName=myProperties.pairName;
	double G=myProperties.shearModulus;
	double lambda=myProperties.bulkModulus-2*G/3;
	double phiPore=myProperties.porosity;
	double phiFrac=myProperties.macroPorosity;
	double KPore=myProperties.permeability;
	double KFrac=myProperties.macroPermeability;

	// Solid properties
	double c_s=1/myProperties.solidBulkModulus;
	double rho_s=myProperties.solidDensity;

	// Fluid properties
	double c_f=1/myProperties.fluidBulkModulus;
	double rho_f=myProperties.fluidDensity;
	double mu_f=myProperties.fluidViscosity;

	// BC types ({u,v,p-pore,p-frac} 1 for Dirichlet and 0 for Neumann, -1 for Stress/Fluid Flow, starts on
	// "north" and follows counterclockwise)
	vector<vector<int>> bcType=
	{
		{-1,-1,1,1},
		{1,-1,-1,-1},
		{-1,1,-1,-1},
		{1,-1,-1,-1}
	};

	// BC values ({u,v,P}, starts on "north" and follows counterclockwise)
	vector<vector<double>> bcValue=
	{
		{0,sigmab,0,0},
		{0,0,0,0},
		{0,0,0,0},
		{0,0,0,0}
	};

/*		GRID CREATION
	----------------------------------------------------------------*/

	// Constructor
	gridDesign myGrid(Nx,Ny,Nt,Lx,Ly,Lt,gridType,sCoordinates);

	// Passing variables
	int Nu;swap(Nu,myGrid.numberOfActiveUDisplacementFV);
	int Nv;swap(Nv,myGrid.numberOfActiveVDisplacementFV);
	int NP;swap(NP,myGrid.numberOfActiveGeneralFV);
	double dx;swap(dx,myGrid.dx);
	double dy;swap(dy,myGrid.dy);
	double dt;swap(dt,myGrid.dt);
	double h;swap(h,myGrid.h);
	vector<vector<int>> idU;swap(idU,myGrid.uDisplacementFVIndex);
	vector<vector<int>> idV;swap(idV,myGrid.vDisplacementFVIndex);
	vector<vector<int>> idP;swap(idP,myGrid.generalFVIndex);
	vector<vector<int>> cooU;swap(cooU,myGrid.uDisplacementFVCoordinates);
	vector<vector<int>> cooV;swap(cooV,myGrid.vDisplacementFVCoordinates);
	vector<vector<int>> cooP;swap(cooP,myGrid.generalFVCoordinates);
	vector<vector<int>> horFaceStatus;swap(horFaceStatus,myGrid.horizontalFacesStatus);
	vector<vector<int>> verFaceStatus;swap(verFaceStatus,myGrid.verticalFacesStatus);
	vector<vector<double>> uField;swap(uField,myGrid.uDisplacementField);
	vector<vector<double>> vField;swap(vField,myGrid.vDisplacementField);
	vector<vector<double>> pPoreField=myGrid.pressureField;
	vector<vector<double>> pFracField=pPoreField;

/*		PROBLEM PARAMETERS CALCULATION
	----------------------------------------------------------------*/

	// Constructor
	problemDoubleParameters myProblem(phiPore,phiFrac,c_f,c_s,G,lambda,mu_f,KPore,KFrac,sigmab,
		cooV,idV);

	// Passing variables
	double psiPore=myProblem.psiPore;
	double psiFrac=myProblem.psiFrac;
	double alpha=myProblem.alpha;
	double S11=myProblem.S11;
	double S12=myProblem.S12;
	double S22=myProblem.S22;
	double leak=myProblem.computeLeakTerm(11);
	double consolidationCoefficient=myProblem.consolCoef;
	
/*		LINEAR SYSTEM'S COEFFICIENTS MATRIX ASSEMBLY
	----------------------------------------------------------------*/

	// Constructor
	coefficientsAssembly myCoefficients(bcType,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);

	// Coefficients matrix assembly
	myCoefficients.assemblyDoublePorosityMatrix(dx,dy,dt,G,lambda,alpha,KPore,KFrac,mu_f,S11,S12,
		S22,psiPore,psiFrac,leak);

	// Passing variables
	vector<vector<double>> coefficientsMatrix;swap(coefficientsMatrix,
		myCoefficients.coefficientsMatrix);
	vector<double> sparseCoefficientsRow;swap(sparseCoefficientsRow,
		myCoefficients.sparseCoefficientsRow);
	vector<double> sparseCoefficientsColumn;swap(sparseCoefficientsColumn,
		myCoefficients.sparseCoefficientsColumn);
	vector<double> sparseCoefficientsValue;swap(sparseCoefficientsValue,
		myCoefficients.sparseCoefficientsValue);

/*		LINEAR SYSTEM SOLVER
	----------------------------------------------------------------*/

	// Variables declaration
	int timeStep;
	vector<double> independentTermsArray;

	// Constructors
	independentTermsAssembly myIndependentTerms(bcType,bcValue,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);
	linearSystemSolver myLinearSystemSolver(coefficientsMatrix,sparseCoefficientsRow,
		sparseCoefficientsColumn,sparseCoefficientsValue,uField,vField,pPoreField,Nu,Nv,NP,Nt,idU,
		idV,idP,cooU,cooV,cooP);

	// Increase the independent terms array
	myIndependentTerms.increaseMacroIndependentTermsArray();

	// Creates macro-pressure field
	myLinearSystemSolver.createMacroPressureField(pFracField);

	// LU Factorization of coefficientsMatrix
	ierr=myLinearSystemSolver.coefficientsMatrixLUFactorization();CHKERRQ(ierr);
	
	// Creation of arrays
	ierr=myLinearSystemSolver.createPETScArrays();CHKERRQ(ierr);
	ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

	for(timeStep=0; timeStep<Nt-1; timeStep++)
	{
		// Assembly of the independent terms array
		myIndependentTerms.assemblyMacroIndependentTermsArray(dx,dy,dt,G,lambda,alpha,KPore,mu_f,
			S11,0,0,uField,vField,pPoreField,pFracField,timeStep,phiPore,phiFrac,KFrac,S12,S22);

		// Passing independent terms array
		independentTermsArray=myIndependentTerms.independentTermsArray;

		// Solution of the linear system
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setRHSValue(independentTermsArray);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.solveLinearSystem();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setFieldValue(timeStep+1);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setMacroFieldValue(timeStep+1);CHKERRQ(ierr);

		// Passing solutions
		uField=myLinearSystemSolver.uField;
		vField=myLinearSystemSolver.vField;
		pPoreField=myLinearSystemSolver.pField;
		pFracField=myLinearSystemSolver.pMField;
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

		cout << timeStep+1<< "\r";
	}

	cout << Ny << "x" << Nx << "x" << Nt-1 << " ";
	cout << "(h=" << h << ", dt=" << dt << ")\n";

/*		DATA PROCESSING
	----------------------------------------------------------------*/
	
	// Variables declaration
	vector<int> exportedTimeSteps=
	{
		{1},
		{(Nt-1)/8},
		{(Nt-1)/2},
		{Nt-1}
	};
	if(Nt==2)
	{
		exportedTimeSteps.clear();
		exportedTimeSteps.push_back(1);
	}

	// Constructor
	dataProcessing myDataProcessing(idU,idV,idP,uField,vField,pPoreField,gridType,interpScheme,dx,
		dy);
	myDataProcessing.storeMacroPressure3DField(idP,pFracField);

	// Exports data for specified time-steps
	for(int i=0; i<exportedTimeSteps.size(); i++)
	{
		myDataProcessing.exportTerzaghiAnalyticalSolution(Ly,psiPore*alpha,1/S11,0,0,0,2*G+lambda,
			sigmab,dt,exportedTimeSteps[i],consolidationCoefficient,pairName);
		myDataProcessing.exportMacroPressureHSolution(dy,h,Ly,exportedTimeSteps[i],pairName);
		myDataProcessing.exportMacroPressureTSolution(dy,dt,Ly,exportedTimeSteps[i],pairName);
	}

	return ierr;
};

int stripfootDouble(string gridType, string interpScheme, int Nt, int meshSize, double Lt,
	double g, double sigmab, poroelasticProperties myProperties)
{
	PetscErrorCode ierr;

/*		ENTRY PARAMETERS
	----------------------------------------------------------------*/

	// Grid parameters
	int Nx=5*meshSize;
	int Ny=5*meshSize;
	int stripSize=meshSize;

	// Reservoir parameters
	double Lx=5; // [m]
	double Ly=5; // [m]

	vector<vector<double>> sCoordinates=
	{
		{Lx,Ly},
		{0,Ly},
		{0,0},
		{Lx,0}
	};

	// Bulk properties
	string pairName=myProperties.pairName;
	double G=myProperties.shearModulus;
	double lambda=myProperties.bulkModulus-2*G/3;
	double phiPore=myProperties.porosity;
	double phiFrac=myProperties.macroPorosity;
	double KPore=myProperties.permeability;
	double KFrac=myProperties.macroPermeability;

	// Solid properties
	double c_s=1/myProperties.solidBulkModulus;
	double rho_s=myProperties.solidDensity;

	// Fluid properties
	double c_f=1/myProperties.fluidBulkModulus;
	double rho_f=myProperties.fluidDensity;
	double mu_f=myProperties.fluidViscosity;

	// BC types ({u,v,p-pore,p-frac} 1 for Dirichlet and 0 for Neumann, -1 for Stress/Fluid Flow, starts on
	// "north" and follows counterclockwise)
	vector<vector<int>> bcType=
	{
		{-1,-1,-1,-1},
		{1,-1,-1,-1},
		{-1,1,-1,-1},
		{1,-1,-1,-1}
	};

	// BC values ({u,v,P}, starts on "north" and follows counterclockwise)
	vector<vector<double>> bcValue=
	{
		{0,0,0,0},
		{0,0,0,0},
		{0,0,0,0},
		{0,0,0,0}
	};

/*		GRID CREATION
	----------------------------------------------------------------*/

	// Constructor
	gridDesign myGrid(Nx,Ny,Nt,Lx,Ly,Lt,gridType,sCoordinates);

	// Passing variables
	int Nu;swap(Nu,myGrid.numberOfActiveUDisplacementFV);
	int Nv;swap(Nv,myGrid.numberOfActiveVDisplacementFV);
	int NP;swap(NP,myGrid.numberOfActiveGeneralFV);
	double dx;swap(dx,myGrid.dx);
	double dy;swap(dy,myGrid.dy);
	double dt;swap(dt,myGrid.dt);
	double h;swap(h,myGrid.h);
	vector<vector<int>> idU;swap(idU,myGrid.uDisplacementFVIndex);
	vector<vector<int>> idV;swap(idV,myGrid.vDisplacementFVIndex);
	vector<vector<int>> idP;swap(idP,myGrid.generalFVIndex);
	vector<vector<int>> cooU;swap(cooU,myGrid.uDisplacementFVCoordinates);
	vector<vector<int>> cooV;swap(cooV,myGrid.vDisplacementFVCoordinates);
	vector<vector<int>> cooP;swap(cooP,myGrid.generalFVCoordinates);
	vector<vector<int>> horFaceStatus;swap(horFaceStatus,myGrid.horizontalFacesStatus);
	vector<vector<int>> verFaceStatus;swap(verFaceStatus,myGrid.verticalFacesStatus);
	vector<vector<double>> uField;swap(uField,myGrid.uDisplacementField);
	vector<vector<double>> vField;swap(vField,myGrid.vDisplacementField);
	vector<vector<double>> pPoreField=myGrid.pressureField;
	vector<vector<double>> pFracField=pPoreField;

/*		PROBLEM PARAMETERS CALCULATION
	----------------------------------------------------------------*/

	// Constructor
	problemDoubleParameters myProblem(phiPore,phiFrac,c_f,c_s,G,lambda,mu_f,KPore,KFrac,sigmab,
		cooV,idV);

	// Passing variables
	double psiPore=myProblem.psiPore;
	double psiFrac=myProblem.psiFrac;
	double alpha=myProblem.alpha;
	double S11=myProblem.S11;
	double S12=myProblem.S12;
	double S22=myProblem.S22;
	double leak=myProblem.computeLeakTerm(11);
	
/*		LINEAR SYSTEM'S COEFFICIENTS MATRIX ASSEMBLY
	----------------------------------------------------------------*/

	// Constructor
	coefficientsAssembly myCoefficients(bcType,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);

	// Coefficients matrix assembly
	myCoefficients.assemblyDoublePorosityMatrix(dx,dy,dt,G,lambda,alpha,KPore,KFrac,mu_f,S11,S12,
		S22,psiPore,psiFrac,leak);
	myCoefficients.addStripfootBC(stripSize,KPore,mu_f);
	myCoefficients.addMacroStripfootBC(stripSize,KFrac,mu_f);

	// Passing variables
	vector<vector<double>> coefficientsMatrix;swap(coefficientsMatrix,
		myCoefficients.coefficientsMatrix);
	vector<double> sparseCoefficientsRow;swap(sparseCoefficientsRow,
		myCoefficients.sparseCoefficientsRow);
	vector<double> sparseCoefficientsColumn;swap(sparseCoefficientsColumn,
		myCoefficients.sparseCoefficientsColumn);
	vector<double> sparseCoefficientsValue;swap(sparseCoefficientsValue,
		myCoefficients.sparseCoefficientsValue);

/*		LINEAR SYSTEM SOLVER
	----------------------------------------------------------------*/

	// Variables declaration
	int timeStep;
	vector<double> independentTermsArray;

	// Constructors
	independentTermsAssembly myIndependentTerms(bcType,bcValue,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);
	linearSystemSolver myLinearSystemSolver(coefficientsMatrix,sparseCoefficientsRow,
		sparseCoefficientsColumn,sparseCoefficientsValue,uField,vField,pPoreField,Nu,Nv,NP,Nt,idU,
		idV,idP,cooU,cooV,cooP);

	// Increase the independent terms array
	myIndependentTerms.increaseMacroIndependentTermsArray();

	// Creates macro-pressure field
	myLinearSystemSolver.createMacroPressureField(pFracField);

	// LU Factorization of coefficientsMatrix
	ierr=myLinearSystemSolver.coefficientsMatrixLUFactorization();CHKERRQ(ierr);
	
	// Creation of arrays
	ierr=myLinearSystemSolver.createPETScArrays();CHKERRQ(ierr);
	ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

	for(timeStep=0; timeStep<Nt-1; timeStep++)
	{
		// Assembly of the independent terms array
		myIndependentTerms.assemblyMacroIndependentTermsArray(dx,dy,dt,G,lambda,alpha,KPore,mu_f,
			S11,0,0,uField,vField,pPoreField,pFracField,timeStep,phiPore,phiFrac,KFrac,S12,S22);
		myIndependentTerms.addStripfootBC(stripSize,dx,sigmab);
		myIndependentTerms.addMacroStripfootBC(stripSize,dx,sigmab);

		// Passing independent terms array
		independentTermsArray=myIndependentTerms.independentTermsArray;

		// Solution of the linear system
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setRHSValue(independentTermsArray);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.solveLinearSystem();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setFieldValue(timeStep+1);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setMacroFieldValue(timeStep+1);CHKERRQ(ierr);

		// Passing solutions
		uField=myLinearSystemSolver.uField;
		vField=myLinearSystemSolver.vField;
		pPoreField=myLinearSystemSolver.pField;
		pFracField=myLinearSystemSolver.pMField;
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

		cout << timeStep+1<< "\r";
	}

	cout << Ny << "x" << Nx << "x" << Nt-1 << " ";
	cout << "(h=" << h << ", dt=" << dt << ")\n";

/*		DATA PROCESSING
	----------------------------------------------------------------*/
	
	// Variables declaration
	vector<int> exportedTimeSteps=
	{
		{1},
		{(Nt-1)/8},
		{(Nt-1)/2},
		{Nt-1}
	};
	if(Nt==2)
	{
		exportedTimeSteps.clear();
		exportedTimeSteps.push_back(1);
	}

	// Constructor
	dataProcessing myDataProcessing(idU,idV,idP,uField,vField,pPoreField,gridType,interpScheme,dx,
		dy);
	myDataProcessing.storeMacroPressure3DField(idP,pFracField);

	// Exports data for specified time-steps
	for(int i=0; i<exportedTimeSteps.size(); i++)
	{
		myDataProcessing.exportStripfootTSolution(dx,dy,dt,Ly,exportedTimeSteps[i],pairName);
		myDataProcessing.exportStripfootHSolution(dx,dy,h,Ly,exportedTimeSteps[i],pairName);
	}
	
	return ierr;
};

int sealedDouble(string gridType, string interpScheme, int Nt, int meshSize, double Lt, double g,
	double sigmab, poroelasticProperties myProperties)
{
	PetscErrorCode ierr;

/*		ENTRY PARAMETERS
	----------------------------------------------------------------*/

	// Grid parameters
	int Nx=meshSize;
	int Ny=6*meshSize;

	// Reservoir parameters
	double Lx=1; // [m]
	double Ly=6; // [m]

	vector<vector<double>> sCoordinates=
	{
		{Lx,Ly},
		{0,Ly},
		{0,0},
		{Lx,0}
	};

	// Bulk properties
	string pairName=myProperties.pairName;
	double G=myProperties.shearModulus;
	double lambda=myProperties.bulkModulus-2*G/3;
	double phiPore=myProperties.porosity;
	double phiFrac=myProperties.macroPorosity;
	double KPore=myProperties.permeability;
	double KFrac=myProperties.macroPermeability;

	// Solid properties
	double c_s=1/myProperties.solidBulkModulus;
	double rho_s=myProperties.solidDensity;

	// Fluid properties
	double c_f=1/myProperties.fluidBulkModulus;
	double rho_f=myProperties.fluidDensity;
	double mu_f=myProperties.fluidViscosity;

	// BC types ({u,v,p-pore,p-frac} 1 for Dirichlet and 0 for Neumann, -1 for Stress/Fluid Flow, starts on
	// "north" and follows counterclockwise)
	vector<vector<int>> bcType=
	{
		{-1,-1,-1,-1},
		{1,-1,-1,-1},
		{-1,1,-1,-1},
		{1,-1,-1,-1}
	};

	// BC values ({u,v,P}, starts on "north" and follows counterclockwise)
	vector<vector<double>> bcValue=
	{
		{0,sigmab,0,0},
		{0,0,0,0},
		{0,0,0,0},
		{0,0,0,0}
	};

/*		GRID CREATION
	----------------------------------------------------------------*/

	// Constructor
	gridDesign myGrid(Nx,Ny,Nt,Lx,Ly,Lt,gridType,sCoordinates);

	// Passing variables
	int Nu;swap(Nu,myGrid.numberOfActiveUDisplacementFV);
	int Nv;swap(Nv,myGrid.numberOfActiveVDisplacementFV);
	int NP;swap(NP,myGrid.numberOfActiveGeneralFV);
	double dx;swap(dx,myGrid.dx);
	double dy;swap(dy,myGrid.dy);
	double dt;swap(dt,myGrid.dt);
	double h;swap(h,myGrid.h);
	vector<vector<int>> idU;swap(idU,myGrid.uDisplacementFVIndex);
	vector<vector<int>> idV;swap(idV,myGrid.vDisplacementFVIndex);
	vector<vector<int>> idP;swap(idP,myGrid.generalFVIndex);
	vector<vector<int>> cooU;swap(cooU,myGrid.uDisplacementFVCoordinates);
	vector<vector<int>> cooV;swap(cooV,myGrid.vDisplacementFVCoordinates);
	vector<vector<int>> cooP;swap(cooP,myGrid.generalFVCoordinates);
	vector<vector<int>> horFaceStatus;swap(horFaceStatus,myGrid.horizontalFacesStatus);
	vector<vector<int>> verFaceStatus;swap(verFaceStatus,myGrid.verticalFacesStatus);
	vector<vector<double>> uField;swap(uField,myGrid.uDisplacementField);
	vector<vector<double>> vField;swap(vField,myGrid.vDisplacementField);
	vector<vector<double>> pPoreField=myGrid.pressureField;
	vector<vector<double>> pFracField=pPoreField;

/*		PROBLEM PARAMETERS CALCULATION
	----------------------------------------------------------------*/

	// Constructor
	problemDoubleParameters myProblem(phiPore,phiFrac,c_f,c_s,G,lambda,mu_f,KPore,KFrac,sigmab,
		cooV,idV);

	// Passing variables
	double psiPore=myProblem.psiPore;
	double psiFrac=myProblem.psiFrac;
	double alpha=myProblem.alpha;
	double S11=myProblem.S11;
	double S12=myProblem.S12;
	double S22=myProblem.S22;
	double leak=myProblem.computeLeakTerm(11);
	
/*		LINEAR SYSTEM'S COEFFICIENTS MATRIX ASSEMBLY
	----------------------------------------------------------------*/

	// Constructor
	coefficientsAssembly myCoefficients(bcType,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);

	// Coefficients matrix assembly
	myCoefficients.assemblyDoublePorosityMatrix(dx,dy,dt,G,lambda,alpha,KPore,KFrac,mu_f,S11,S12,
		S22,psiPore,psiFrac,leak);

	// Passing variables
	vector<vector<double>> coefficientsMatrix;swap(coefficientsMatrix,
		myCoefficients.coefficientsMatrix);
	vector<double> sparseCoefficientsRow;swap(sparseCoefficientsRow,
		myCoefficients.sparseCoefficientsRow);
	vector<double> sparseCoefficientsColumn;swap(sparseCoefficientsColumn,
		myCoefficients.sparseCoefficientsColumn);
	vector<double> sparseCoefficientsValue;swap(sparseCoefficientsValue,
		myCoefficients.sparseCoefficientsValue);

/*		LINEAR SYSTEM SOLVER
	----------------------------------------------------------------*/

	// Variables declaration
	int timeStep;
	vector<double> independentTermsArray;

	// Constructors
	independentTermsAssembly myIndependentTerms(bcType,bcValue,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);
	linearSystemSolver myLinearSystemSolver(coefficientsMatrix,sparseCoefficientsRow,
		sparseCoefficientsColumn,sparseCoefficientsValue,uField,vField,pPoreField,Nu,Nv,NP,Nt,idU,
		idV,idP,cooU,cooV,cooP);

	// Increase the independent terms array
	myIndependentTerms.increaseMacroIndependentTermsArray();

	// Creates macro-pressure field
	myLinearSystemSolver.createMacroPressureField(pFracField);

	// LU Factorization of coefficientsMatrix
	ierr=myLinearSystemSolver.coefficientsMatrixLUFactorization();CHKERRQ(ierr);
	
	// Creation of arrays
	ierr=myLinearSystemSolver.createPETScArrays();CHKERRQ(ierr);
	ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

	for(timeStep=0; timeStep<Nt-1; timeStep++)
	{
		// Assembly of the independent terms array
		myIndependentTerms.assemblyMacroIndependentTermsArray(dx,dy,dt,G,lambda,alpha,KPore,mu_f,
			S11,0,0,uField,vField,pPoreField,pFracField,timeStep,phiPore,phiFrac,KFrac,S12,S22);

		// Passing independent terms array
		independentTermsArray=myIndependentTerms.independentTermsArray;

		// Solution of the linear system
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setRHSValue(independentTermsArray);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.solveLinearSystem();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setFieldValue(timeStep+1);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setMacroFieldValue(timeStep+1);CHKERRQ(ierr);

		// Passing solutions
		uField=myLinearSystemSolver.uField;
		vField=myLinearSystemSolver.vField;
		pPoreField=myLinearSystemSolver.pField;
		pFracField=myLinearSystemSolver.pMField;
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

		cout << timeStep+1<< "\r";
	}

	cout << Ny << "x" << Nx << "x" << Nt-1 << " ";
	cout << "(h=" << h << ", dt=" << dt << ")\n";

/*		DATA PROCESSING
	----------------------------------------------------------------*/
	
	// Variables declaration
	vector<int> exportedTimeSteps=
	{
		// {1},
		// {(Nt-1)/16},
		// {(Nt-1)/8},
		{Nt-1}
	};
	if(Nt==2)
	{
		exportedTimeSteps.clear();
		exportedTimeSteps.push_back(1);
	}

	// Constructor
	doubleDataProcessing myDataProcessing(idU,idV,idP,uField,vField,pPoreField,gridType,
		interpScheme,dx,dy);
	myDataProcessing.storeMacroPressure3DField(idP,pFracField);

	// Exports data for specified time-steps
	for(int i=0; i<exportedTimeSteps.size(); i++)
	{
		myDataProcessing.exportSealedDoubleAnalyticalSolution(Ly,alpha*psiPore,alpha*psiFrac,
			2*G+lambda,S11,S12,S22,sigmab,dt,exportedTimeSteps[i],pairName);
		myDataProcessing.exportSealedDoubleNumericalSolution(dy,dt,Ly,exportedTimeSteps[i],
			pairName);
	}

	return ierr;
};

int storageDouble(string gridType, string interpScheme, int Nt, int meshSize, double Lt, double g,
	double sigmab, poroelasticProperties myProperties)
{
	PetscErrorCode ierr;

/*		ENTRY PARAMETERS
	----------------------------------------------------------------*/

	// Grid parameters
	int Nx=meshSize;
	int Ny=6*meshSize;

	// Reservoir parameters
	double Lx=1; // [m]
	double Ly=6; // [m]

	vector<vector<double>> sCoordinates=
	{
		{Lx,Ly},
		{0,Ly},
		{0,0},
		{Lx,0}
	};

	// Bulk properties
	string pairName=myProperties.pairName;
	double G=myProperties.shearModulus;
	double lambda=myProperties.bulkModulus-2*G/3;
	double phiPore=myProperties.porosity;
	double phiFrac=myProperties.macroPorosity;
	double KPore=myProperties.permeability;
	double KFrac=myProperties.macroPermeability;

	// Solid properties
	double c_s=1/myProperties.solidBulkModulus;
	double rho_s=myProperties.solidDensity;

	// Fluid properties
	double c_f=1/myProperties.fluidBulkModulus;
	double rho_f=myProperties.fluidDensity;
	double mu_f=myProperties.fluidViscosity;

	// BC types ({u,v,p-pore,p-frac} 1 for Dirichlet and 0 for Neumann, -1 for Stress/Fluid Flow, starts on
	// "north" and follows counterclockwise)
	vector<vector<int>> bcType=
	{
		{-1,-1,1,1},
		{1,-1,-1,-1},
		{-1,1,-1,-1},
		{1,-1,-1,-1}
	};

	// BC values ({u,v,P}, starts on "north" and follows counterclockwise)
	vector<vector<double>> bcValue=
	{
		{0,sigmab,0,0},
		{0,0,0,0},
		{0,0,0,0},
		{0,0,0,0}
	};

/*		GRID CREATION
	----------------------------------------------------------------*/

	// Constructor
	gridDesign myGrid(Nx,Ny,Nt,Lx,Ly,Lt,gridType,sCoordinates);

	// Passing variables
	int Nu;swap(Nu,myGrid.numberOfActiveUDisplacementFV);
	int Nv;swap(Nv,myGrid.numberOfActiveVDisplacementFV);
	int NP;swap(NP,myGrid.numberOfActiveGeneralFV);
	double dx;swap(dx,myGrid.dx);
	double dy;swap(dy,myGrid.dy);
	double dt;swap(dt,myGrid.dt);
	double h;swap(h,myGrid.h);
	vector<vector<int>> idU;swap(idU,myGrid.uDisplacementFVIndex);
	vector<vector<int>> idV;swap(idV,myGrid.vDisplacementFVIndex);
	vector<vector<int>> idP;swap(idP,myGrid.generalFVIndex);
	vector<vector<int>> cooU;swap(cooU,myGrid.uDisplacementFVCoordinates);
	vector<vector<int>> cooV;swap(cooV,myGrid.vDisplacementFVCoordinates);
	vector<vector<int>> cooP;swap(cooP,myGrid.generalFVCoordinates);
	vector<vector<int>> horFaceStatus;swap(horFaceStatus,myGrid.horizontalFacesStatus);
	vector<vector<int>> verFaceStatus;swap(verFaceStatus,myGrid.verticalFacesStatus);
	vector<vector<double>> uField;swap(uField,myGrid.uDisplacementField);
	vector<vector<double>> vField;swap(vField,myGrid.vDisplacementField);
	vector<vector<double>> pPoreField=myGrid.pressureField;
	vector<vector<double>> pFracField=pPoreField;

/*		PROBLEM PARAMETERS CALCULATION
	----------------------------------------------------------------*/

	// Constructor
	problemDoubleParameters myProblem(phiPore,phiFrac,c_f,c_s,G,lambda,mu_f,KPore,KFrac,sigmab,
		cooV,idV);

	// Apply initial conditions
	myProblem.applyDoublePoreInitialConditions(dy,Ly,vField,pPoreField,pFracField);

	// Passing variables
	double psiPore=myProblem.psiPore;
	double psiFrac=myProblem.psiFrac;
	double alpha=myProblem.alpha;
	double S11=myProblem.S11;
	double S12=myProblem.S12;
	double S22=myProblem.S22;
	double leak=myProblem.computeLeakTerm(0);
	vField=myProblem.vDisplacementField;
	pPoreField=myProblem.pressurePoreField;
	pFracField=myProblem.pressureFracField;
	
/*		LINEAR SYSTEM'S COEFFICIENTS MATRIX ASSEMBLY
	----------------------------------------------------------------*/

	// Constructor
	coefficientsAssembly myCoefficients(bcType,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);

	// Coefficients matrix assembly
	myCoefficients.assemblyDoublePorosityMatrix(dx,dy,dt,G,lambda,alpha,KPore,KFrac,mu_f,S11,S12,
		S22,psiPore,psiFrac,leak);

	// Passing variables
	vector<vector<double>> coefficientsMatrix;swap(coefficientsMatrix,
		myCoefficients.coefficientsMatrix);
	vector<double> sparseCoefficientsRow;swap(sparseCoefficientsRow,
		myCoefficients.sparseCoefficientsRow);
	vector<double> sparseCoefficientsColumn;swap(sparseCoefficientsColumn,
		myCoefficients.sparseCoefficientsColumn);
	vector<double> sparseCoefficientsValue;swap(sparseCoefficientsValue,
		myCoefficients.sparseCoefficientsValue);

/*		LINEAR SYSTEM SOLVER
	----------------------------------------------------------------*/

	// Variables declaration
	int timeStep;
	vector<double> independentTermsArray;

	// Constructors
	independentTermsAssembly myIndependentTerms(bcType,bcValue,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);
	linearSystemSolver myLinearSystemSolver(coefficientsMatrix,sparseCoefficientsRow,
		sparseCoefficientsColumn,sparseCoefficientsValue,uField,vField,pPoreField,Nu,Nv,NP,Nt,idU,
		idV,idP,cooU,cooV,cooP);

	// Increase the independent terms array
	myIndependentTerms.increaseMacroIndependentTermsArray();

	// Creates macro-pressure field
	myLinearSystemSolver.createMacroPressureField(pFracField);

	// LU Factorization of coefficientsMatrix
	ierr=myLinearSystemSolver.coefficientsMatrixLUFactorization();CHKERRQ(ierr);
	
	// Creation of arrays
	ierr=myLinearSystemSolver.createPETScArrays();CHKERRQ(ierr);
	ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

	for(timeStep=0; timeStep<Nt-1; timeStep++)
	{
		// Assembly of the independent terms array
		myIndependentTerms.assemblyMacroIndependentTermsArray(dx,dy,dt,G,lambda,alpha,KPore,mu_f,
			S11,0,0,uField,vField,pPoreField,pFracField,timeStep,phiPore,phiFrac,KFrac,S12,S22);

		// Passing independent terms array
		independentTermsArray=myIndependentTerms.independentTermsArray;

		// Solution of the linear system
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setRHSValue(independentTermsArray);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.solveLinearSystem();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setFieldValue(timeStep+1);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setMacroFieldValue(timeStep+1);CHKERRQ(ierr);

		// Passing solutions
		uField=myLinearSystemSolver.uField;
		vField=myLinearSystemSolver.vField;
		pPoreField=myLinearSystemSolver.pField;
		pFracField=myLinearSystemSolver.pMField;
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

		cout << timeStep+1<< "\r";
	}

	cout << Ny << "x" << Nx << "x" << Nt-1 << " ";
	cout << "(h=" << h << ", dt=" << dt << ")\n";

/*		DATA PROCESSING
	----------------------------------------------------------------*/
	
	// Variables declaration
	vector<int> exportedTimeSteps=
	{
		{1},
		{2},
		{3},
		{4},
		{125},
		{250},
		{500}
	};
	if(Nt==2)
	{
		exportedTimeSteps.clear();
		exportedTimeSteps.push_back(1);
	}

	// Constructor
	doubleDataProcessing myDataProcessing(idU,idV,idP,uField,vField,pPoreField,gridType,
		interpScheme,dx,dy);
	myDataProcessing.storeMacroPressure3DField(idP,pFracField);

	// Exports data for specified time-steps
	for(int i=0; i<exportedTimeSteps.size(); i++)
	{
		myDataProcessing.exportDrainedDoubleNumericalSolution(dy,dt,Ly,exportedTimeSteps[i],
			pairName);
		myDataProcessing.exportStorageAnalyticalSolution(Ly,alpha*psiPore,alpha*psiFrac,
			2*G+lambda,S11,S12,S22,KPore,KFrac,mu_f,sigmab,dt,exportedTimeSteps[i],pairName);
	}

	return ierr;
};

int leakingDouble(string gridType, string interpScheme, int Nt, int meshSize, double Lt, double g,
	double sigmab, poroelasticProperties myProperties)
{
	PetscErrorCode ierr;

/*		ENTRY PARAMETERS
	----------------------------------------------------------------*/

	// Grid parameters
	int Nx=meshSize;
	int Ny=6*meshSize;

	// Reservoir parameters
	double Lx=1; // [m]
	double Ly=6; // [m]

	vector<vector<double>> sCoordinates=
	{
		{Lx,Ly},
		{0,Ly},
		{0,0},
		{Lx,0}
	};

	// Bulk properties
	string pairName=myProperties.pairName;
	double G=myProperties.shearModulus;
	double lambda=myProperties.bulkModulus-2*G/3;
	double phiPore=myProperties.porosity;
	double phiFrac=myProperties.macroPorosity;
	double KPore=myProperties.permeability;
	double KFrac=myProperties.macroPermeability;

	// Solid properties
	double c_s=1/myProperties.solidBulkModulus;
	double rho_s=myProperties.solidDensity;

	// Fluid properties
	double c_f=1/myProperties.fluidBulkModulus;
	double rho_f=myProperties.fluidDensity;
	double mu_f=myProperties.fluidViscosity;

	// BC types ({u,v,p-pore,p-frac} 1 for Dirichlet and 0 for Neumann, -1 for Stress/Fluid Flow, starts on
	// "north" and follows counterclockwise)
	vector<vector<int>> bcType=
	{
		{-1,-1,1,1},
		{1,-1,-1,-1},
		{-1,1,-1,-1},
		{1,-1,-1,-1}
	};

	// BC values ({u,v,P}, starts on "north" and follows counterclockwise)
	vector<vector<double>> bcValue=
	{
		{0,sigmab,0,0},
		{0,0,0,0},
		{0,0,0,0},
		{0,0,0,0}
	};

/*		GRID CREATION
	----------------------------------------------------------------*/

	// Constructor
	gridDesign myGrid(Nx,Ny,Nt,Lx,Ly,Lt,gridType,sCoordinates);

	// Passing variables
	int Nu;swap(Nu,myGrid.numberOfActiveUDisplacementFV);
	int Nv;swap(Nv,myGrid.numberOfActiveVDisplacementFV);
	int NP;swap(NP,myGrid.numberOfActiveGeneralFV);
	double dx;swap(dx,myGrid.dx);
	double dy;swap(dy,myGrid.dy);
	double dt;swap(dt,myGrid.dt);
	double h;swap(h,myGrid.h);
	vector<vector<int>> idU;swap(idU,myGrid.uDisplacementFVIndex);
	vector<vector<int>> idV;swap(idV,myGrid.vDisplacementFVIndex);
	vector<vector<int>> idP;swap(idP,myGrid.generalFVIndex);
	vector<vector<int>> cooU;swap(cooU,myGrid.uDisplacementFVCoordinates);
	vector<vector<int>> cooV;swap(cooV,myGrid.vDisplacementFVCoordinates);
	vector<vector<int>> cooP;swap(cooP,myGrid.generalFVCoordinates);
	vector<vector<int>> horFaceStatus;swap(horFaceStatus,myGrid.horizontalFacesStatus);
	vector<vector<int>> verFaceStatus;swap(verFaceStatus,myGrid.verticalFacesStatus);
	vector<vector<double>> uField;swap(uField,myGrid.uDisplacementField);
	vector<vector<double>> vField;swap(vField,myGrid.vDisplacementField);
	vector<vector<double>> pPoreField=myGrid.pressureField;
	vector<vector<double>> pFracField=pPoreField;

/*		PROBLEM PARAMETERS CALCULATION
	----------------------------------------------------------------*/

	// Constructor
	problemDoubleParameters myProblem(phiPore,phiFrac,c_f,c_s,G,lambda,mu_f,KPore,KFrac,sigmab,
		cooV,idV);

	// Apply initial conditions
	myProblem.applyDoublePoreInitialConditions(dy,Ly,vField,pPoreField,pFracField);

	// Passing variables
	double psiPore=myProblem.psiPore;
	double psiFrac=myProblem.psiFrac;
	double alpha=myProblem.alpha;
	double S11=myProblem.S11;
	double S12=myProblem.S12;
	double S22=myProblem.S22;
	double leak=myProblem.computeLeakTerm(11);
	vField=myProblem.vDisplacementField;
	pPoreField=myProblem.pressurePoreField;
	pFracField=myProblem.pressureFracField;

	// Forcing transient term decoupling
	double M=2*G+lambda;
	S12=-psiPore*alpha*psiFrac*alpha/M;
	
/*		LINEAR SYSTEM'S COEFFICIENTS MATRIX ASSEMBLY
	----------------------------------------------------------------*/

	// Constructor
	coefficientsAssembly myCoefficients(bcType,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);

	// Coefficients matrix assembly
	myCoefficients.assemblyDoublePorosityMatrix(dx,dy,dt,G,lambda,alpha,KPore,KFrac,mu_f,S11,S12,
		S22,psiPore,psiFrac,leak);

	// Passing variables
	vector<vector<double>> coefficientsMatrix;swap(coefficientsMatrix,
		myCoefficients.coefficientsMatrix);
	vector<double> sparseCoefficientsRow;swap(sparseCoefficientsRow,
		myCoefficients.sparseCoefficientsRow);
	vector<double> sparseCoefficientsColumn;swap(sparseCoefficientsColumn,
		myCoefficients.sparseCoefficientsColumn);
	vector<double> sparseCoefficientsValue;swap(sparseCoefficientsValue,
		myCoefficients.sparseCoefficientsValue);

/*		LINEAR SYSTEM SOLVER
	----------------------------------------------------------------*/

	// Variables declaration
	int timeStep;
	vector<double> independentTermsArray;

	// Constructors
	independentTermsAssembly myIndependentTerms(bcType,bcValue,Nu,Nv,NP,idU,idV,idP,cooU,cooV,cooP,
		horFaceStatus,verFaceStatus,gridType,interpScheme);
	linearSystemSolver myLinearSystemSolver(coefficientsMatrix,sparseCoefficientsRow,
		sparseCoefficientsColumn,sparseCoefficientsValue,uField,vField,pPoreField,Nu,Nv,NP,Nt,idU,
		idV,idP,cooU,cooV,cooP);

	// Increase the independent terms array
	myIndependentTerms.increaseMacroIndependentTermsArray();

	// Creates macro-pressure field
	myLinearSystemSolver.createMacroPressureField(pFracField);

	// LU Factorization of coefficientsMatrix
	ierr=myLinearSystemSolver.coefficientsMatrixLUFactorization();CHKERRQ(ierr);
	
	// Creation of arrays
	ierr=myLinearSystemSolver.createPETScArrays();CHKERRQ(ierr);
	ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

	for(timeStep=0; timeStep<Nt-1; timeStep++)
	{
		// Assembly of the independent terms array
		myIndependentTerms.assemblyMacroIndependentTermsArray(dx,dy,dt,G,lambda,alpha,KPore,mu_f,
			S11,0,0,uField,vField,pPoreField,pFracField,timeStep,phiPore,phiFrac,KFrac,S12,S22);

		// Passing independent terms array
		independentTermsArray=myIndependentTerms.independentTermsArray;

		// Solution of the linear system
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setRHSValue(independentTermsArray);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.solveLinearSystem();CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setFieldValue(timeStep+1);CHKERRQ(ierr);
		ierr=myLinearSystemSolver.setMacroFieldValue(timeStep+1);CHKERRQ(ierr);

		// Passing solutions
		uField=myLinearSystemSolver.uField;
		vField=myLinearSystemSolver.vField;
		pPoreField=myLinearSystemSolver.pField;
		pFracField=myLinearSystemSolver.pMField;
		ierr=myLinearSystemSolver.zeroPETScArrays();CHKERRQ(ierr);

		cout << timeStep+1<< "\r";
	}

	cout << Ny << "x" << Nx << "x" << Nt-1 << " ";
	cout << "(h=" << h << ", dt=" << dt << ")\n";

/*		DATA PROCESSING
	----------------------------------------------------------------*/
	
	// Variables declaration
	vector<int> exportedTimeSteps=
	{
		{1},
		{62},
		{125},
		{500}
	};
	if(Nt==2)
	{
		exportedTimeSteps.clear();
		exportedTimeSteps.push_back(1);
	}

	// Constructor
	doubleDataProcessing myDataProcessing(idU,idV,idP,uField,vField,pPoreField,gridType,
		interpScheme,dx,dy);
	myDataProcessing.storeMacroPressure3DField(idP,pFracField);

	// Exports data for specified time-steps
	for(int i=0; i<exportedTimeSteps.size(); i++)
	{
		myDataProcessing.exportDrainedDoubleNumericalSolution(dy,dt,Ly,exportedTimeSteps[i],
			pairName);
		myDataProcessing.exportLeakingAnalyticalSolution(Ly,alpha*psiPore,alpha*psiFrac,
			2*G+lambda,S11,S12,S22,KPore,KFrac,mu_f,sigmab,leak,dt,exportedTimeSteps[i],pairName);
	}

	return ierr;
};