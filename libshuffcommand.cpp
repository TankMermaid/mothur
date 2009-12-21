/*
 *  libshuffcommand.cpp
 *  Mothur
 *
 *  Created by Sarah Westcott on 3/9/09.
 *  Copyright 2009 Schloss Lab UMASS Amherst. All rights reserved.
 *
 */

/* This class is designed to implement an integral form of the Cramer-von Mises statistic.
	you may refer to the "Integration of Microbial Ecology and Statistics: A Test To Compare Gene Libraries" 
	paper in Applied and Environmental Microbiology, Sept. 2004, p. 5485-5492 0099-2240/04/$8.00+0  
	DOI: 10.1128/AEM.70.9.5485-5492.2004 Copyright 2004 American Society for Microbiology for more information. */


#include "libshuffcommand.h"
#include "libshuff.h"
#include "slibshuff.h"
#include "dlibshuff.h"

//**********************************************************************************************************************

LibShuffCommand::LibShuffCommand(string option){
	try {
		globaldata = GlobalData::getInstance();
		abort = false;
		Groups.clear();
		
		
		//allow user to run help
		if(option == "help") { help(); abort = true; }
		
		else {
			//valid paramters for this command
			string Array[] =  {"iters","groups","step","form","cutoff"};
			vector<string> myArray (Array, Array+(sizeof(Array)/sizeof(string)));
			
			OptionParser parser(option);
			map<string, string> parameters = parser.getParameters();
			
			ValidParameters validParameter;
		
			//check to make sure all parameters are valid for command
			for (map<string,string>::iterator it = parameters.begin(); it != parameters.end(); it++) { 
				if (validParameter.isValidParameter(it->first, myArray, it->second) != true) {  abort = true;  }
			}
			
			//make sure the user has already run the read.dist command
			if ((globaldata->gMatrix == NULL) || (globaldata->gGroupmap == NULL)) {
				mothurOut("You must read in a matrix and groupfile using the read.dist command, before you use the libshuff command. "); mothurOutEndLine(); abort = true;; 
			}
						
			//check for optional parameter and set defaults
			// ...at some point should added some additional type checking...
			groups = validParameter.validFile(parameters, "groups", false);			
			if (groups == "not found") { groups = ""; savegroups = groups; }
			else { 
				savegroups = groups;
				splitAtDash(groups, Groups);
				globaldata->Groups = Groups;
			}
				
			string temp;
			temp = validParameter.validFile(parameters, "iters", false);				if (temp == "not found") { temp = "10000"; }
			convert(temp, iters); 
			
			temp = validParameter.validFile(parameters, "cutoff", false);				if (temp == "not found") { temp = "1.0"; }
			convert(temp, cutOff); 
			
			temp = validParameter.validFile(parameters, "step", false);				if (temp == "not found") { temp = "0.01"; }
			convert(temp, step); 
	
			userform = validParameter.validFile(parameters, "form", false);			if (userform == "not found") { userform = "integral"; }
			
			if (abort == false) {
		
				matrix = globaldata->gMatrix;				//get the distance matrix
				setGroups();								//set the groups to be analyzed and sorts them
	
				/********************************************************************************************/
				//this is needed because when we read the matrix we sort it into groups in alphabetical order
				//the rest of the command and the classes used in this command assume specific order
				/********************************************************************************************/
				matrix->setGroups(globaldata->gGroupmap->namesOfGroups);
				vector<int> sizes;
				for (int i = 0; i < globaldata->gGroupmap->namesOfGroups.size(); i++) {   sizes.push_back(globaldata->gGroupmap->getNumSeqs(globaldata->gGroupmap->namesOfGroups[i]));  }
				matrix->setSizes(sizes);
			

				if(userform == "discrete"){
					form = new DLibshuff(matrix, iters, step, cutOff);
				}
				else{
					form = new SLibshuff(matrix, iters, cutOff);
				}
			}
			
		}
		
	}
	catch(exception& e) {
		errorOut(e, "LibShuffCommand", "LibShuffCommand");
		exit(1);
	}
}
//**********************************************************************************************************************

void LibShuffCommand::help(){
	try {
		mothurOut("The libshuff command can only be executed after a successful read.dist command including a groupfile.\n");
		mothurOut("The libshuff command parameters are groups, iters, step, form and cutoff.  No parameters are required.\n");
		mothurOut("The groups parameter allows you to specify which of the groups in your groupfile you would like analyzed.  You must enter at least 2 valid groups.\n");
		mothurOut("The group names are separated by dashes.  The iters parameter allows you to specify how many random matrices you would like compared to your matrix.\n");
		mothurOut("The step parameter allows you to specify change in distance you would like between each output if you are using the discrete form.\n");
		mothurOut("The form parameter allows you to specify if you would like to analyze your matrix using the discrete or integral form. Your options are integral or discrete.\n");
		mothurOut("The libshuff command should be in the following format: libshuff(groups=yourGroups, iters=yourIters, cutOff=yourCutOff, form=yourForm, step=yourStep).\n");
		mothurOut("Example libshuff(groups=A-B-C, iters=500, form=discrete, step=0.01, cutOff=2.0).\n");
		mothurOut("The default value for groups is all the groups in your groupfile, iters is 10000, cutoff is 1.0, form is integral and step is 0.01.\n");
		mothurOut("The libshuff command output two files: .coverage and .slsummary their descriptions are in the manual.\n");
		mothurOut("Note: No spaces between parameter labels (i.e. iters), '=' and parameters (i.e.yourIters).\n\n");
	}
	catch(exception& e) {
		errorOut(e, "LibShuffCommand", "help");
		exit(1);
	}
}

//**********************************************************************************************************************

int LibShuffCommand::execute(){
	try {
		
		if (abort == true) {	return 0;	}
	
		savedDXYValues = form->evaluateAll();
		savedMinValues = form->getSavedMins();
	
		pValueCounts.resize(numGroups);
		for(int i=0;i<numGroups;i++){
			pValueCounts[i].assign(numGroups, 0);
		}
		
		Progress* reading = new Progress();
		
		for(int i=0;i<numGroups-1;i++) {
			for(int j=i+1;j<numGroups;j++) {
				reading->newLine(groupNames[i]+'-'+groupNames[j], iters);
				int spoti = globaldata->gGroupmap->groupIndex[groupNames[i]]; //neccessary in case user selects groups so you know where they are in the matrix
				int spotj = globaldata->gGroupmap->groupIndex[groupNames[j]];
	
				for(int p=0;p<iters;p++) {		
					form->randomizeGroups(spoti,spotj); 
					if(form->evaluatePair(spoti,spotj) >= savedDXYValues[spoti][spotj])	{	pValueCounts[i][j]++;	}
					if(form->evaluatePair(spotj,spoti) >= savedDXYValues[spotj][spoti])	{	pValueCounts[j][i]++;	}
					reading->update(p);			
				}
				form->resetGroup(spoti);
				form->resetGroup(spotj);
			}
		}
		reading->finish();
		delete reading;

		mothurOutEndLine();
		printSummaryFile();
		printCoverageFile();
		
		//clear out users groups
		globaldata->Groups.clear();
		delete form;
		
		//delete globaldata's copy of the gmatrix to free up memory
		delete globaldata->gMatrix;  globaldata->gMatrix = NULL;
		
		return 0;
	}
	catch(exception& e) {
		errorOut(e, "LibShuffCommand", "execute");
		exit(1);
	}
}

//**********************************************************************************************************************

void LibShuffCommand::printCoverageFile() {
	try {

		ofstream outCov;
		summaryFile = getRootName(globaldata->getPhylipFile()) + "libshuff.coverage";
		openOutputFile(summaryFile, outCov);
		outCov.setf(ios::fixed, ios::floatfield); outCov.setf(ios::showpoint);
		//cout.setf(ios::fixed, ios::floatfield); cout.setf(ios::showpoint);
		
		map<double,vector<int> > allDistances;
		map<double,vector<int> >::iterator it;

		vector<vector<int> > indices(numGroups);
		int numIndices = numGroups * numGroups;
		
		int index = 0;
		for(int i=0;i<numGroups;i++){
			indices[i].assign(numGroups,0);
			for(int j=0;j<numGroups;j++){
				indices[i][j] = index++;
				
				int spoti = globaldata->gGroupmap->groupIndex[groupNames[i]]; //neccessary in case user selects groups so you know where they are in the matrix
				int spotj = globaldata->gGroupmap->groupIndex[groupNames[j]];
				
				for(int k=0;k<savedMinValues[spoti][spotj].size();k++){
					if(allDistances[savedMinValues[spoti][spotj][k]].size() != 0){
						allDistances[savedMinValues[spoti][spotj][k]][indices[i][j]]++;
					}
					else{
						allDistances[savedMinValues[spoti][spotj][k]].assign(numIndices, 0);
						allDistances[savedMinValues[spoti][spotj][k]][indices[i][j]] = 1;
					}
				}
			}
		}
		it=allDistances.begin();
		
		//cout << setprecision(8);

		vector<int> prevRow = it->second;
		it++;
		
		for(;it!=allDistances.end();it++){
			for(int i=0;i<it->second.size();i++){
				it->second[i] += prevRow[i];
			}
			prevRow = it->second;
		}
		
		vector<int> lastRow = allDistances.rbegin()->second;
		outCov << setprecision(8);
		
		outCov << "dist";
		for (int i = 0; i < numGroups; i++){
			outCov << '\t' << groupNames[i];
		}
		for (int i=0;i<numGroups;i++){
			for(int j=i+1;j<numGroups;j++){
				outCov << '\t' << groupNames[i] << '-' << groupNames[j] << '\t';
				outCov << groupNames[j] << '-' << groupNames[i];
			}
		}
		outCov << endl;
		
		for(it=allDistances.begin();it!=allDistances.end();it++){
			outCov << it->first << '\t';
			for(int i=0;i<numGroups;i++){
				outCov << it->second[indices[i][i]]/(float)lastRow[indices[i][i]] << '\t';
			}
			for(int i=0;i<numGroups;i++){
				for(int j=i+1;j<numGroups;j++){
					outCov << it->second[indices[i][j]]/(float)lastRow[indices[i][j]] << '\t';
					outCov << it->second[indices[j][i]]/(float)lastRow[indices[j][i]] << '\t';
				}
			}
			outCov << endl;
		}
		outCov.close();
	}
	catch(exception& e) {
		errorOut(e, "LibShuffCommand", "printCoverageFile");
		exit(1);
	}
} 

//**********************************************************************************************************************

void LibShuffCommand::printSummaryFile() {
	try {

		ofstream outSum;
		summaryFile = getRootName(globaldata->getPhylipFile()) + "libshuff.summary";
		openOutputFile(summaryFile, outSum);

		outSum.setf(ios::fixed, ios::floatfield); outSum.setf(ios::showpoint);
		cout.setf(ios::fixed, ios::floatfield); cout.setf(ios::showpoint);
		
		cout << setw(20) << left << "Comparison" << '\t' << setprecision(8) << "dCXYScore" << '\t' << "Significance" << endl;
		mothurOutJustToLog("Comparison\tdCXYScore\tSignificance"); mothurOutEndLine();
		outSum << setw(20) << left << "Comparison" << '\t' << setprecision(8) << "dCXYScore" << '\t' << "Significance" << endl;
	
		int precision = (int)log10(iters);
		for(int i=0;i<numGroups;i++){
			for(int j=i+1;j<numGroups;j++){
				int spoti = globaldata->gGroupmap->groupIndex[groupNames[i]]; //neccessary in case user selects groups so you know where they are in the matrix
				int spotj = globaldata->gGroupmap->groupIndex[groupNames[j]];
				
				if(pValueCounts[i][j]){
					cout << setw(20) << left << groupNames[i]+'-'+groupNames[j] << '\t' << setprecision(8) << savedDXYValues[spoti][spotj] << '\t' << setprecision(precision) << pValueCounts[i][j]/(float)iters << endl;
					mothurOutJustToLog(groupNames[i]+"-"+groupNames[j] + "\t" + toString(savedDXYValues[spoti][spotj]) + "\t" + toString((pValueCounts[i][j]/(float)iters))); mothurOutEndLine();
					outSum << setw(20) << left << groupNames[i]+'-'+groupNames[j] << '\t' << setprecision(8) << savedDXYValues[spoti][spotj] << '\t' << setprecision(precision) << pValueCounts[i][j]/(float)iters << endl;
				}
				else{
					cout << setw(20) << left << groupNames[i]+'-'+groupNames[j] << '\t' << setprecision(8) << savedDXYValues[spoti][spotj] << '\t' << '<' <<setprecision(precision) << 1/(float)iters << endl;
					mothurOutJustToLog(groupNames[i]+"-"+groupNames[j] + "\t" + toString(savedDXYValues[spoti][spotj]) + "\t" + toString((1/(float)iters))); mothurOutEndLine();
					outSum << setw(20) << left << groupNames[i]+'-'+groupNames[j] << '\t' << setprecision(8) << savedDXYValues[spoti][spotj] << '\t' << '<' <<setprecision(precision) << 1/(float)iters << endl;
				}
				if(pValueCounts[j][i]){
					cout << setw(20) << left << groupNames[j]+'-'+groupNames[i] << '\t' << setprecision(8) << savedDXYValues[spotj][spoti] << '\t' << setprecision (precision) << pValueCounts[j][i]/(float)iters << endl;
					mothurOutJustToLog(groupNames[j]+"-"+groupNames[i] + "\t" + toString(savedDXYValues[spotj][spoti]) + "\t" + toString((pValueCounts[j][i]/(float)iters))); mothurOutEndLine();
					outSum << setw(20) << left << groupNames[j]+'-'+groupNames[i] << '\t' << setprecision(8) << savedDXYValues[spotj][spoti] << '\t' << setprecision (precision) << pValueCounts[j][i]/(float)iters << endl;
				}
				else{
					cout << setw(20) << left << groupNames[j]+'-'+groupNames[i] << '\t' << setprecision(8) << savedDXYValues[spotj][spoti] << '\t' << '<' <<setprecision (precision) << 1/(float)iters << endl;
					mothurOutJustToLog(groupNames[j]+"-"+groupNames[i] + "\t" + toString(savedDXYValues[spotj][spoti]) + "\t" + toString((1/(float)iters))); mothurOutEndLine();
					outSum << setw(20) << left << groupNames[j]+'-'+groupNames[i] << '\t' << setprecision(8) << savedDXYValues[spotj][spoti] << '\t' << '<' <<setprecision (precision) << 1/(float)iters << endl;
				}
			}
		}
		
		outSum.close();
	}
	catch(exception& e) {
		errorOut(e, "LibShuffCommand", "printSummaryFile");
		exit(1);
	}
} 

//**********************************************************************************************************************

void LibShuffCommand::setGroups() {
	try {
		//if the user has not entered specific groups to analyze then do them all
		if (globaldata->Groups.size() == 0) {
			numGroups = globaldata->gGroupmap->getNumGroups();
			for (int i=0; i < numGroups; i++) { 
				globaldata->Groups.push_back(globaldata->gGroupmap->namesOfGroups[i]);
			}
		} else {
			if (savegroups != "all") {
				//check that groups are valid
				for (int i = 0; i < globaldata->Groups.size(); i++) {
					if (globaldata->gGroupmap->isValidGroup(globaldata->Groups[i]) != true) {
						mothurOut(globaldata->Groups[i] + " is not a valid group, and will be disregarded."); mothurOutEndLine();
						// erase the invalid group from globaldata->Groups
						globaldata->Groups.erase(globaldata->Groups.begin()+i);
					}
				}
			
				//if the user only entered invalid groups
				if ((globaldata->Groups.size() == 0) || (globaldata->Groups.size() == 1)) { 
					numGroups = globaldata->gGroupmap->getNumGroups();
					for (int i=0; i < numGroups; i++) { 
						globaldata->Groups.push_back(globaldata->gGroupmap->namesOfGroups[i]);
					}
					mothurOut("When using the groups parameter you must have at least 2 valid groups. I will run the command using all the groups in your groupfile."); mothurOutEndLine();
				} else { numGroups = globaldata->Groups.size(); }
			} else { //users wants all groups
				numGroups = globaldata->gGroupmap->getNumGroups();
				globaldata->Groups.clear();
				for (int i=0; i < numGroups; i++) { 
					globaldata->Groups.push_back(globaldata->gGroupmap->namesOfGroups[i]);
				}
			}
		}

		//sort so labels match
		sort(globaldata->Groups.begin(), globaldata->Groups.end());
		
		//sort
		sort(globaldata->gGroupmap->namesOfGroups.begin(), globaldata->gGroupmap->namesOfGroups.end());
		
		for (int i = 0; i < globaldata->gGroupmap->namesOfGroups.size(); i++) {  globaldata->gGroupmap->groupIndex[globaldata->gGroupmap->namesOfGroups[i]] = i;  }

		groupNames = globaldata->Groups;

	}
	catch(exception& e) {
		errorOut(e, "LibShuffCommand", "setGroups");
		exit(1);
	}
}

/***********************************************************/
