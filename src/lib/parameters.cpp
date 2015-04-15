/*
 * parameters.cpp
 *
 *  Created on: 3 Apr, 2015
 *      Author: Nok
 */

#include "../header/parameters.h"
#include "../header/sampling.h"
#include <cmath>
#include <algorithm>
#include <iostream>

void findParameters(vector<JSDivergence>* jsDivergences, vector<int>* Ls,
		vector<int>* Ds, vector<double>* Fs, int totalGenes,
		GeneExpression* geneExpression, Mutations* mutations,
		TIntAdjList* network, int numSamples) {

	vector<int>* genesEx = geneExpression->genes;
	vector<int>* genesMut = mutations->genes;

	TDoubleMatrix* originalGeneExpressionMatrix = geneExpression->matrix;
	TIntegerMatrix* originalMutationMatrix = mutations->matrix;
	int totalSamples = originalMutationMatrix->at(0).size();

	int numLs = Ls->size();
	int numDs = Ds->size();
	int numFs = Fs->size();

	//ignore the choice (of F) in which the median number of deregulated genes is more than half of the gene in the network or <300
	double halfNumberOfGenesInNetwork = totalGenes / 2;
	vector<double> medianNumberOfDeregulatedGenes(Fs->size());
	for (unsigned i = 0; i < Fs->size(); ++i) {
		double F = Fs->at(i);
		medianNumberOfDeregulatedGenes[i] = getMedianNumberOfDeregulatedGenes(
				originalGeneExpressionMatrix, F);
	}

	int count = 0;	//count number of combinations

	for (int li = 0; li < numLs; ++li) {
		for (int di = 0; di < numDs; ++di) {
			for (int fi = 0; fi < numFs; ++fi) {

				int L = Ls->at(li);
				int D = Ds->at(di);
				double F = Fs->at(fi);

				cout << "\tcurrent parameters (L, D, F) is " << L << ", " << D
						<< ", " << F << endl;

				//save values
				jsDivergences->at(count).L = L;
				jsDivergences->at(count).D = D;
				jsDivergences->at(count).F = F;

				//ignore the choice (of F) in which the median number of deregulated genes is more than half of the gene in the network or <300
				if (medianNumberOfDeregulatedGenes[fi] > halfNumberOfGenesInNetwork
						or medianNumberOfDeregulatedGenes[fi] < 300) {
					jsDivergences->at(count).divergence = 0;
					cout << "\t\tskipped this set of parameters\n";
					break;
				}

				/*
				 * calculated JS divergence
				 */

				//100 iterations to generate the frequency distribution and compute JS divergence
				vector< vector<int> >* realDistributionAll = new vector< vector<int> >;
				vector< vector<int> >* randomDistributionAll = new vector< vector<int> >;

				cout << "\t\tcreating frequency distribution...";

				//TODO move the resampling to the loop of 100 rounds
				/*
				 * resample numSamples samples for every round
				 */

				//list of samples id to be used for tuning the parameters
				vector<int> rrank(totalSamples);
				createPermutation(&rrank);	//return a permutation of [0, totalSamples-1]

				//TODO create sub matrix for case of < 50 samples (just skip this part and use the original dataset)
				//probably this can be done by pre-generating 100 sub matrix (all sets of params will use the same 100 sub matrix)

				//gene expression submatrix
				TDoubleMatrix subGeneExpressionMatrix;
				GeneExpression subGeneExpression;
				subGeneExpression.genes = genesEx;	// the same set of genes as the original gene expression matrix
				subGeneExpression.matrix = &subGeneExpressionMatrix;	//subset of samples
				randomlyChooseSamplesDouble(originalGeneExpressionMatrix,
						&subGeneExpressionMatrix, &rrank, numSamples);

				//mutation submatrix
				TIntegerMatrix subMutationMatrix;
				Mutations subMutations;
				subMutations.genes = genesMut;	// the same set of genes as the combined mutation matrix
				subMutations.matrix = &subMutationMatrix;
				randomlyChooseSamplesInt(originalMutationMatrix, &subMutationMatrix,
						&rrank, numSamples);


				//count the number of times that the frequency is greater then the real frequency
				int round = 100;

				int progress = 1;
				int interval = round / 100;

				for (int i = 0; i < round; ++i) {

					//print progression
					if (i % interval == 0) {
						const string progStatus = intToStr(progress) + "%";
						cout << progStatus << flush;
						progress++;
						cout << string(progStatus.length(), '\b');
					}


					/*
					 * find explained genes for real samples
					 */

					//find explained genes for real sub-sample (without gene label permutation)
					vector<int> realDistribution(totalGenes * 2);//differentiate the up and down regulated genes
					int sampleId = 0; //the first sample
					for (; sampleId < numSamples; sampleId++) {
						//cout << "Sample #" << sampleId << endl;
						vector<double> sampleGeneExpression(totalGenes); //expression of all genes in the network
						getGeneExpressionFromSampleId(&subGeneExpressionMatrix,
								genesEx, &sampleGeneExpression, sampleId);

						vector<int> mutatedGeneIds; // to store gene id of mutated genes
						getMutatedGeneIdsFromSampleId(&subMutations,
								&mutatedGeneIds, sampleId, genesMut);

						vector<int> explainedGenesFrequency(totalGenes * 2);//differentiate the up and down regulated genes
						getExplainedGenesIdOnlyUpDown(&explainedGenesFrequency,
								network, &sampleGeneExpression, &mutatedGeneIds,
								L, D, F);

						//update real distribution
						for (int j = 0; j < totalGenes * 2; ++j) {//differentiate the up and down regulated genes
							if (explainedGenesFrequency[j] > 0) {
								realDistribution[j]++;
							}
						}
					}
					realDistributionAll->push_back(realDistribution);

//					cout << "calculated distribution of real samples" << endl;

					/*
					 * find explained genes for random sub-sample (with gene label permutation)
					 */

					//repermutate the gene label of random a dataset
					//Create gene label permutation for both gene expression and mutation matrix
					//1. gene expression
					vector<int> permutedGeneLabelsEx;
					permuteGeneLabels(genesEx, &permutedGeneLabelsEx);
					//2. mutation
					vector<int> permutedGeneLabelsMut;
					permuteGeneLabels(genesMut, &permutedGeneLabelsMut);

					vector<int> randomDistribution(totalGenes * 2); //differentiate the up and down regulated genes
					sampleId = 0; //the first sample
					for (; sampleId < numSamples; sampleId++) {

						vector<double> sampleGeneExpression(totalGenes); // of all genes
						getGeneExpressionFromSampleId(&subGeneExpressionMatrix,
								&permutedGeneLabelsEx, &sampleGeneExpression,
								sampleId);

						vector<int> mutatedGeneIds; // to store gene id of mutated genes
						getMutatedGeneIdsFromSampleId(&subMutations,
								&mutatedGeneIds, sampleId,
								&permutedGeneLabelsMut);

						vector<int> explainedGenesFrequency(totalGenes * 2);//differentiate the up and down regulated genes
						getExplainedGenesIdOnlyUpDown(&explainedGenesFrequency,
								network, &sampleGeneExpression, &mutatedGeneIds,
								L, D, F);

						//update random distribution
						for (int j = 0; j < totalGenes * 2; ++j) {//differentiate the up and down regulated genes
							if (explainedGenesFrequency[j] > 0) {
								randomDistribution[j]++;
							}
						}

					}
					randomDistributionAll->push_back(randomDistribution);

				}	//end for loop of 100 round

				cout << endl;	//for progression printing

				cout << "calculated distribution of random samples" << endl;

				cout << "\t\tcalculating JS divergence..." << endl;

				//calculate JS divergence
				//TODO ERROR double free or corruption while returning from the function
//				double divergence = calculateJSDivergence(realDistributionAll, randomDistributionAll, numSamples);

				jsDivergences->at(count).divergence = calculateJSDivergence(realDistributionAll, randomDistributionAll, numSamples);
				cout << "\t\tcalculated JS divergence..." << endl;
				cout << "\t\tsaved JS divergence..." << endl;

				delete realDistributionAll;
				delete randomDistributionAll;

				//Note: sometimes the divergence is not calculated because of the constraint of the number of deregulated genes
//				cout << "\t\tJS divergence = " << divergence << endl;
				count++;

			} //end for loop for Fs
		} //end for loop for Ds
	}  //end for loop for Ls
}

double calculateJSDivergence(const vector<vector<int> >* realDistributionAll,
		const vector<vector<int> >* randomDistributionAll, int numSamples) {
	int round = randomDistributionAll->size();
	int totalGenes = randomDistributionAll->at(0).size();

	//1. create frequency distribution (x: #number of samples y: frequency) for both real and random samples
	vector<int> randomFrequencyDistribution(numSamples);
	vector<int> realFrequencyDistribution(numSamples);
	// for each round
	for (int i = 0; i < round; ++i) {
		//for each genes, get the frequency
		for (int j = 0; j < totalGenes; ++j) {
			int frequencyOfAGeneRandom = randomDistributionAll->at(i)[j];
			int frequencyOfAGeneReal = realDistributionAll->at(i)[j];
			//add the frequency to distribution
			randomFrequencyDistribution[frequencyOfAGeneRandom]++;
			realFrequencyDistribution[frequencyOfAGeneReal]++;
		}
	}

	cout << "\t\tcreated frequency distribution...\n";

	//2. compute divergence
	//from perl code
	//$js += $P_v * log( 2 * $P_v / ( $P_v + $Q_v ) ) / $log_2 if($P_v != 0);   # P Log ( P /(P+Q)/2 )
	//$js += $Q_v * log( 2 * $Q_v / ( $P_v + $Q_v ) ) / $log_2 if($Q_v != 0);   # Q Log ( Q /(P+Q)/2 )

	double jsDivergence = 0;
	for (int i = 0; i < numSamples; ++i) {
		double pi = realFrequencyDistribution[i];
		double qi = randomFrequencyDistribution[i];
		if (qi > 0 or pi > 0) {
			if (pi != 0) {	//real frequency is zero
				jsDivergence += pi * log(pi / ((pi + qi) / 2));
			}
			if (qi != 0) {	//random frequency is zero
				jsDivergence += qi * log(qi / ((pi + qi) / 2));
			}
		}
	}

	cout << "\t\tcomputed JS divergence = " << jsDivergence / 2 << endl;

	return jsDivergence / 2;
}

double getMedianNumberOfDeregulatedGenes(TDoubleMatrix* geneExpressionMatrix,
		double F) {
	int numSamples = geneExpressionMatrix->at(0).size();
	int numGenes = geneExpressionMatrix->size();

	//for each sample i
	vector<int> counts;
	for (int i = 0; i < numSamples; ++i) {
		//count deregulated genes
		int count = 0;
		//for each gene j
		for (int j = 0; j < numGenes; ++j) {
			if (fabs(geneExpressionMatrix->at(j)[i]) > F) {
				count++;
			}
		}
		counts.push_back(count);
	}

	//find the median number of deregulated genes across samples
	sort(counts.begin(), counts.end());
	double median;
	if (counts.size() % 2 == 0) {
		median = (counts[counts.size() / 2 - 1] + counts[counts.size() / 2])
				/ 2.0;
	} else {
		median = counts[counts.size() / 2];
	}

	return median;
}

void findMaximumJsDivergence(vector<JSDivergence>* jsDivergences,
		JSDivergence* maxJs) {
	int total = jsDivergences->size();
	double max = 0;
	for (int i = 0; i < total; ++i) {
		if (jsDivergences->at(i).divergence > max) {
			maxJs->D = jsDivergences->at(i).D;
			maxJs->L = jsDivergences->at(i).L;
			maxJs->F = jsDivergences->at(i).F;
			maxJs->divergence = jsDivergences->at(i).divergence;
			max = maxJs->divergence;
		}
	}
}

