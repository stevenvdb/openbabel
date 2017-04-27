/**********************************************************************
Copyright (C) 2001-2006 by Geoffrey R. Hutchison
Some portions Copyright (C) 2004 by Chris Morley
Some portions Copyright (C) 2009 by Michael Banck
Some portions Copyright (C) 2014 by Dagmar Lenk

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
***********************************************************************/
#include <openbabel/babelconfig.h>

#include <openbabel/obmolecformat.h>
#include <regex.h>
#include <iomanip>

#define notFound string::npos
using namespace std;
namespace OpenBabel
{

  class OrcaOutputFormat : public OBMoleculeFormat
  {
  public:
    //Register this format type ID
    OrcaOutputFormat()
    {
      OBConversion::RegisterFormat("orca",this);
    }

    virtual const char* Description() //required
    {
      return
        "Orca output format\n"
        "Read Options e.g. -as\n"
        " s  Output single bonds only\n"
        " b  Disable bonding entirely\n\n";
    }

    virtual const char* SpecificationURL()
    {return "http://www.cec.mpg.de/forum/portal.php";} //optional

    //Flags() can return be any the following combined by | or be omitted if none apply
    // NOTREADABLE  READONEONLY  NOTWRITABLE  WRITEONEONLY
    virtual unsigned int Flags()
    {
      return READONEONLY | NOTWRITABLE;
    }

    ////////////////////////////////////////////////////
    /// The "API" interface functions
    virtual bool ReadMolecule(OBBase* pOb, OBConversion* pConv);

    string checkColumns(string tmp);
  };

  //Make an instance of the format class
  OrcaOutputFormat theOrcaOutputFormat;

  class OrcaInputFormat : public OBMoleculeFormat
  {
  public:
    //Register this format type ID
    OrcaInputFormat()
    {
      OBConversion::RegisterFormat("orcainp",this);
    }

    virtual const char* Description() //required
    {
      return
        "Orca input format\n"
        "This can be used as a template file for orca calculations\n";
    }

    virtual const char* SpecificationURL()
    {return"http://www.cec.mpg.de/forum/portal.php";} //optional

    //Flags() can return be any the following combined by | or be omitted if none apply
    // NOTREADABLE  READONEONLY  NOTWRITABLE  WRITEONEONLY
    virtual unsigned int Flags()
    {
      return NOTREADABLE | WRITEONEONLY;
    }

    ////////////////////////////////////////////////////
    /// The "API" interface functions
    virtual bool WriteMolecule(OBBase* pOb, OBConversion* pConv);

  };

  //Make an instance of the format class
  OrcaInputFormat theOrcaInputFormat;


  /////////////////////////////////////////////////////////////////
  bool OrcaOutputFormat::ReadMolecule(OBBase* pOb, OBConversion* pConv)
  {

    OBMol* pmol = pOb->CastAndClear<OBMol>();
    if(pmol==NULL)
      return false;

    //Define some references so we can use the old parameter names
    istream &ifs = *pConv->GetInStream();
    OBMol &mol = *pmol;
    const char* title = pConv->GetTitle();


    // molecule energy
    double energy=0;
    //Vibrational data
    std::vector< std::vector< vector3 > > Lx;
    std::vector<double> Frequencies, Intensities, RamanActivities, UVWavelength, UVForces, UVEDipole;
    std::vector<double> CDWavelength, CDVelosity, CDStrengthsLength;
    // frequencies and normal modes
    std::vector<double> FrequenciesAll;
    int nModeAll = 0;

    //MO data
    bool m_openShell = false;
    std::vector<double>  energyEh, energyeV;
    std::vector<double>  occ;
    std::vector<double>  energyBEh, energyBeV;
    std::vector<double>  occB;

    // Unit cell
    bool unitCell = false;
    std::vector<vector3> unitCellVectors;

    bool hasPartialCharges = false;

    char buffer[BUFF_SIZE];
    string str;
    double x,y,z;
    OBAtom *atom;
    vector<string> vs;

    mol.BeginModify();
    while	(ifs.getline(buffer,BUFF_SIZE)) {

        string checkKeywords(buffer);
        if (checkKeywords.find("CARTESIAN COORDINATES (ANGSTROEM)") != notFound) {
            //        if(strstr(buffer,"CARTESIAN COORDINATES (ANGSTROEM)") != NULL) {
            if (unitCell) break; // dont't overwrite unit cell coordinate informations
            mol.Clear();

            ifs.getline(buffer,BUFF_SIZE);	// ---- ----- ----
            ifs.getline(buffer,BUFF_SIZE);
            tokenize(vs,buffer);
            while (vs.size() == 4) {
                atom = mol.NewAtom();
                x = atof((char*)vs[1].c_str());
                y = atof((char*)vs[2].c_str());
                z = atof((char*)vs[3].c_str());
                atom->SetVector(x,y,z); //set coordinates

                //set atomic number
                atom->SetAtomicNum(etab.GetAtomicNum(vs[0].c_str()));

                if (!ifs.getline(buffer,BUFF_SIZE))
                  break;
                tokenize(vs,buffer);
            }
        } // if "output coordinates"

        if (checkKeywords.find("ORBITAL ENERGIES") != notFound) {
//        if(strstr(buffer,"ORBITAL ENERGIES") != NULL) {
            energyEh.resize(0);
            energyeV.resize(0);
            occ.resize(0);
            ifs.getline(buffer,BUFF_SIZE); // skip ---------------------
            ifs.getline(buffer,BUFF_SIZE); // skip empty line or look for spin informations
            if (strstr(buffer,"SPIN UP ORBITALS") != NULL) m_openShell = true;
            ifs.getline(buffer,BUFF_SIZE); // skip headline
            ifs.getline(buffer,BUFF_SIZE);
            tokenize(vs,buffer);
            while (strstr(buffer,"---------") == NULL && vs.size() !=0) {
                if (vs.size() != 4) break;
                occ.push_back(atof(vs[1].c_str()));
                energyEh.push_back(atof(vs[2].c_str()));
                energyeV.push_back(atof(vs[3].c_str()));
                ifs.getline(buffer,BUFF_SIZE);
                tokenize(vs,buffer);
            }
            if (m_openShell) {
                energyBEh.resize(0);
                energyBeV.resize(0);
                occB.resize(0);

                ifs.getline(buffer,BUFF_SIZE); // skip spin informations
                ifs.getline(buffer,BUFF_SIZE); // skip headline
                ifs.getline(buffer,BUFF_SIZE);
                tokenize(vs,buffer);
                while (strstr(buffer,"---------") == NULL && vs.size() >0) {
                    if (vs.size() != 4) break;
                    occB.push_back(atof(vs[1].c_str()));
                    energyBEh.push_back(atof(vs[2].c_str()));
                    energyBeV.push_back(atof(vs[3].c_str()));
                    ifs.getline(buffer,BUFF_SIZE);
                    tokenize(vs,buffer);
                }
            }
        } // if "ORBITAL ENERGIES"
        if (checkKeywords.find("Total Charge") != notFound) {

            //get total charge

            tokenize(vs,buffer);
            if (vs.size() == 5) {
                mol.SetTotalCharge (atoi(vs[4].c_str()));
            }

            // get Multiplicity

            ifs.getline(buffer,BUFF_SIZE);
            tokenize(vs,buffer);
            if (vs.size() == 4) {
                mol.SetTotalSpinMultiplicity(atoi(vs[3].c_str()));
            }
        }
        if (checkKeywords.find("MULLIKEN ATOMIC CHARGES") != notFound) {
            hasPartialCharges = true;
            ifs.getline(buffer,BUFF_SIZE);	// skip --------------
            ifs.getline(buffer,BUFF_SIZE);
            tokenize(vs,buffer);
            //  std::cout << "charges "  << buffer << endl;

            while (vs.size() == 4)
            { // atom number, atomic symbol,:,  charge

                atom = mol.GetAtom(atoi(vs[0].c_str())+1);  // Numbering starts from 0 in Orca
                atom->SetPartialCharge(atof(vs[3].c_str()));

                if (!ifs.getline(buffer,BUFF_SIZE))
                    break;
                tokenize(vs,buffer);
            }
        }
        if (checkKeywords.find("FINAL SINGLE POINT ENERGY") != notFound) {
            tokenize(vs,buffer);
            if (vs.size() == 5) mol.SetEnergy(atof(vs[4].c_str()));
        }

        if (checkKeywords.find("VIBRATIONAL FREQUENCIES") != notFound) {
            FrequenciesAll.resize(0);
            ifs.getline(buffer,BUFF_SIZE);      // skip ----------
            ifs.getline(buffer,BUFF_SIZE);      // skip empty line
            ifs.getline(buffer,BUFF_SIZE);
            tokenize(vs,buffer);
            while (vs.size() >1) {
                FrequenciesAll.push_back(atof(vs[1].c_str()));
                ifs.getline(buffer,BUFF_SIZE);
                tokenize(vs,buffer);
            }
            nModeAll = FrequenciesAll.size();

        } // if "VIBRATIONAL FREQUENCIES"

        if (checkKeywords.find("NORMAL MODES") != notFound) {

            Lx.resize(0);
            for (unsigned int i=0;i<6;i++) {
                ifs.getline(buffer,BUFF_SIZE);     // skip ----------, comments and blank lines
            }

            ifs.getline(buffer,BUFF_SIZE);     // header line
            tokenize(vs,buffer);
            int iMode = 0;
            while (vs.size() != 0) {
                int nColumn = vs.size();
                vector<vector<vector3> > vib;
                ifs.getline(buffer,BUFF_SIZE);
                str = checkColumns (string(buffer));
                tokenize(vs,str);
                while(vs.size() == nColumn+1) {
                    vector<double> x, y, z;
                    for (unsigned int i = 1; i < vs.size(); i++)
                        x.push_back(atof(vs[i].c_str()));
                    ifs.getline(buffer, BUFF_SIZE);
                    str = checkColumns (string(buffer));
                    tokenize(vs,str);
                    for (unsigned int i = 1; i < vs.size(); i++)
                        y.push_back(atof(vs[i].c_str()));
                    ifs.getline(buffer, BUFF_SIZE);
                    str = checkColumns (string(buffer));
                    tokenize(vs,str);
                    for (unsigned int i = 1; i < vs.size(); i++)
                        z.push_back(atof(vs[i].c_str()));

                    for (unsigned int i = 0; i < nColumn; i++) {
                        vib.push_back(vector<vector3>());
                        vib[i].push_back(vector3(x[i], y[i], z[i]));
                    }

//                    std::cout <<" vib.size = "<< vib.size() << endl;
                    ifs.getline(buffer, BUFF_SIZE);
                    str = checkColumns (string(buffer));
                    tokenize(vs,str);
                } // while
//                std::cout <<" end while vib.size = "<< vib.size() << endl;
//                for (unsigned int i = iMode; i < iMode+nColumn; i++) {
                for (unsigned int i = 0; i < nColumn; i++) {
//                    std::cout << "orca i = "  << i << endl;
                    if (FrequenciesAll[iMode] > 10.0) { // something higher than 0
//                        std::cout <<" vib[i].size = " <<i << " " << vib[i].size() << endl;
                        Lx.push_back(vib[i]);
//                        std::cout << i<< "  " << Lx[i].size() << endl;
//                        std::cout << Lx.size() << endl;
                    }
                    iMode++;
                }
            } // while
        } // if "NORMAL MODES"}

        if (checkKeywords.find("IR SPECTRUM") != notFound) {
            Frequencies.resize(0);
            Intensities.resize(0);

            ifs.getline(buffer, BUFF_SIZE); // skip ---------------------
            ifs.getline(buffer, BUFF_SIZE); // skip empty line
            ifs.getline(buffer, BUFF_SIZE); // skip header
            ifs.getline(buffer, BUFF_SIZE); // skip ---------------------
            ifs.getline(buffer, BUFF_SIZE);
            tokenize(vs,buffer);

            while (vs.size() == 7) {
                //                std::cout << (atof(vs[1].c_str())) << endl;
                //                std::cout << (atof(vs[2].c_str())) << endl;
                Frequencies.push_back(atof(vs[1].c_str()));
                Intensities.push_back(atof(vs[2].c_str()));
                ifs.getline(buffer, BUFF_SIZE);
                tokenize(vs,buffer);
            }
        } // if "IR SPECTRUM"
        if (checkKeywords.find("RAMAN SPECTRUM") != notFound) {
//        if(strstr(buffer,"RAMAN SPECTRUM") != NULL)
//        {
            RamanActivities.resize(0);
            ifs.getline(buffer, BUFF_SIZE); // skip ---------------------
            ifs.getline(buffer, BUFF_SIZE); // skip empty line
            ifs.getline(buffer, BUFF_SIZE); // skip header
            ifs.getline(buffer, BUFF_SIZE); // skip ---------------------
            ifs.getline(buffer, BUFF_SIZE);
            tokenize(vs,buffer);

            while (vs.size() == 4 ) {
                RamanActivities.push_back(atof(vs[2].c_str()));
                ifs.getline(buffer, BUFF_SIZE);
                tokenize(vs,buffer);
            }
        } // if "RAMAN SPECTRUM"

        if (checkKeywords.find("ABSORPTION SPECTRUM VIA TRANSITION ELECTRIC DIPOLE MOMENTS") != notFound) {
//        if(strstr(buffer,"ABSORPTION SPECTRUM VIA TRANSITION ELECTRIC DIPOLE MOMENTS") != NULL)
//        {
            UVWavelength.resize(0);
            UVForces.resize(0);
            UVEDipole.resize(0);
            ifs.getline(buffer, BUFF_SIZE); // skip ---------------------
            ifs.getline(buffer, BUFF_SIZE); // skip header
            ifs.getline(buffer, BUFF_SIZE); // skip header
            ifs.getline(buffer, BUFF_SIZE); // skip ---------------------
            ifs.getline(buffer, BUFF_SIZE);
            tokenize(vs,buffer);

            while (vs.size() == 8) {
                UVForces.push_back(0.0);        // ORCA doesn't have these values
                UVWavelength.push_back(atof(vs[2].c_str()));
                UVEDipole.push_back(atof(vs[3].c_str()));
                ifs.getline(buffer, BUFF_SIZE);
                tokenize(vs,buffer);
            }
        } // if "ABSORPTION SPECTRUM VIA TRANSITION ELECTRIC DIPOLE MOMENTS"

        if (checkKeywords.find("CD SPECTRUM") != notFound) {
//        if(strstr(buffer,"CD SPECTRUM") != NULL)
//        {
            std::cout << "CD spectrum found" << endl;
            CDWavelength.resize(0);
            CDVelosity.resize(0);
            CDStrengthsLength.resize(0);
            ifs.getline(buffer, BUFF_SIZE); // skip ---------------------
            ifs.getline(buffer, BUFF_SIZE); // skip header
            ifs.getline(buffer, BUFF_SIZE); // skip header
            ifs.getline(buffer, BUFF_SIZE); // skip ---------------------
            ifs.getline(buffer, BUFF_SIZE);
            tokenize(vs,buffer);

            while (vs.size() == 7) {
                CDVelosity.push_back(0.0);        // ORCA doesn't calculate these values
                CDWavelength.push_back(atof(vs[2].c_str()));
                CDStrengthsLength.push_back(atof(vs[3].c_str()));
                ifs.getline(buffer, BUFF_SIZE);
                tokenize(vs,buffer);
            }
            std::cout << CDWavelength.size() << endl;
            std::cout << CDStrengthsLength.size() << endl;
        } // if "CD SPECTRUM"

        if (checkKeywords.find("UNIT CELL (ANGSTROM)") != notFound) { // file contains unit cell information
            unitCellVectors.resize(0);

            ifs.getline(buffer,BUFF_SIZE);
            tokenize(vs,buffer);
            while (vs.size() == 4) {
                x = atof((char*)vs[1].c_str());
                y = atof((char*)vs[2].c_str());
                z = atof((char*)vs[3].c_str());
                unitCellVectors.push_back(vector3 (x,y,z)); //set coordinates

                if (!ifs.getline(buffer,BUFF_SIZE))
                    break;
                tokenize(vs,buffer);
            }
            if (unitCellVectors.size()!=4 )
                break;      // structure incorrect

            if (!ifs.getline(buffer,BUFF_SIZE))
                break;

            // look for coordinate information relating to the unit cell calculations

            string checkNextKeyword(buffer);
            if (checkNextKeyword.find("CARTESIAN COORDINATES (ANGSTROM)") != notFound){
                mol.Clear();

                ifs.getline(buffer,BUFF_SIZE);
                tokenize(vs,buffer);
                while (vs.size() >= 4) { // sometime there are additional infos in the line
                    atom = mol.NewAtom();
                    x = atof((char*)vs[1].c_str());
                    y = atof((char*)vs[2].c_str());
                    z = atof((char*)vs[3].c_str());
                    atom->SetVector(x,y,z); //set coordinates

                    //set atomic number
                    atom->SetAtomicNum(etab.GetAtomicNum(vs[0].c_str()));

                    if (!ifs.getline(buffer,BUFF_SIZE))
                        break;
                    tokenize(vs,buffer);
                }
            } // if "unit cell related coordinates"
            if (mol.NumAtoms() != 0)
                unitCell = true;
        } // if "unit cell information"

    } // while

    if (mol.NumAtoms() == 0) {
      mol.EndModify();
      return false;
    }

    // Attach unit cell if any

    if (unitCell) {
        OBUnitCell *uC = new OBUnitCell;

        uC->SetData(unitCellVectors.at(0), unitCellVectors.at(1), unitCellVectors.at(2));
        uC->SetOffset(unitCellVectors.at(3));
        mol.SetData(uC);
    }

    // Attach orbital data if any

    if (energyEh.size() > 0){
        OBOrbitalData *od = new OBOrbitalData();

        std::vector<OBOrbital> alphaOrbitals;
        int alphaHomo, betaHomo = 0;
        for (unsigned int i = 0; i < energyEh.size(); i++) {
            if (occ[i]>0) alphaHomo++;
            OBOrbital orb;
            orb.SetData(energyEh[i], occ[i], " ");
            alphaOrbitals.push_back(orb);
        }
        od->SetAlphaOrbitals (alphaOrbitals);

        if (m_openShell) {
            std::vector<OBOrbital> betaOrbitals;

            for (unsigned int i = 0; i < energyBEh.size(); i++) {
                if (occ[i]>0) betaHomo++;
                OBOrbital orb;
                orb.SetData(energyBEh[i], occB[i], " ");
                betaOrbitals.push_back(orb);
            }
            od->SetBetaOrbitals (betaOrbitals);
        }
        od->SetHOMO(alphaHomo,betaHomo);
        od->SetOrigin(fileformatInput);
        mol.SetData(od);
    }

    //Attach vibrational data, if there are any, to molecule
    if(Frequencies.size()>0)
    {
        OBVibrationData* vd = new OBVibrationData;
        if (RamanActivities.size() != 0) {
            vd->SetData(Lx, Frequencies, Intensities, RamanActivities);
        } else {
            vd->SetData(Lx, Frequencies, Intensities);
        }
        mol.SetData(vd);
    }

    // Attach UV / CD spectra data if there are any

    if(UVWavelength.size() > 0 || CDWavelength.size() > 0)
    {
        OBElectronicTransitionData* etd = new OBElectronicTransitionData;

        if (UVWavelength.size() > 0) {
            // UV spectrum has been found
            etd->SetData(UVWavelength, UVForces);
            if (UVEDipole.size() == UVWavelength.size())
                etd->SetEDipole(UVEDipole);
            // additional CD spectrum has also been found
            if (CDWavelength.size() == UVWavelength.size()) {
                etd->SetRotatoryStrengthsLength(CDStrengthsLength);
                etd->SetRotatoryStrengthsVelocity(CDVelosity); // just vector with 0.0 because ORCA doesn't calculate these values
            }
        } else {
            // only CD spectrum has been found
            etd->SetData(CDWavelength, CDVelosity); // ony wavelengths information are known , 2nd vector just contains 0.0
            etd->SetRotatoryStrengthsLength(CDStrengthsLength);
            etd->SetRotatoryStrengthsVelocity(CDVelosity); // just vector with 0.0 because ORCA doesn't calculate these values
        }
        etd->SetOrigin(fileformatInput);
        mol.SetData(etd);
    }


    if (!pConv->IsOption("b",OBConversion::INOPTIONS))
      mol.ConnectTheDots();
    if (!pConv->IsOption("s",OBConversion::INOPTIONS) && !pConv->IsOption("b",OBConversion::INOPTIONS))
      mol.PerceiveBondOrders();

    mol.EndModify();

    if (hasPartialCharges)
      mol.SetPartialChargesPerceived();
    mol.SetTitle(title);
    return(true);
  }

  ////////////////////////////////////////////////////////////////

  bool OrcaInputFormat::WriteMolecule(OBBase* pOb, OBConversion* pConv)
  {
    OBMol* pmol = dynamic_cast<OBMol*>(pOb);
    if(pmol==NULL)
      return false;

    //Define some references so we can use the old parameter names
    ostream &ofs = *pConv->GetOutStream();
    OBMol &mol = *pmol;

    char buffer[BUFF_SIZE];

    ofs << "# ORCA input file" << endl;
    ofs << "# " << mol.GetTitle() << endl;
    ofs << "! insert inline commands here " << endl;
    ofs << "* xyz " << mol.GetTotalCharge() << " " << mol.GetTotalSpinMultiplicity() << endl;


    FOR_ATOMS_OF_MOL(atom, mol)
    {
        ofs << setw(4) << right
            << OpenBabel::etab.GetSymbol(atom->GetAtomicNum())
            << setw(15) << setprecision(5) << fixed << showpoint
            << right << atom->GetX() << " " << setw(15) << atom->GetY() << " "
            << setw(15) << atom->GetZ() << endl;
    }

    ofs << "*" << endl;

    return(true);
  }

// small function to avoid wrong parsing
// if there is no whitespace between the numbers in the column structure

  string OrcaOutputFormat::checkColumns(string checkBuffer)
  {
      string pattern ("[0-9]-");
      regmatch_t pm;
      regex_t myregex;
      int pos = regcomp(&myregex, pattern.c_str(), REG_EXTENDED);
      if (pos !=0) return (checkBuffer); // do nothing

      while (regexec(&myregex, checkBuffer.c_str(), 1, &pm, REG_EXTENDED) == 0) {
          checkBuffer.insert(pm.rm_eo-1, " ");  // insert whitespace to seperate the columns
      }
      return (checkBuffer);
  }
} //namespace OpenBabel
