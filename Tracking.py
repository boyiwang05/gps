#!/usr/bin/env python3

# This is based off of (essentially a Python port of) the tracking.m file included with SoftGNSS v3.0.
# The license that was included with that program is below:

#------------------------Original License----------------------------------
#                           SoftGNSS v3.0
#
# Copyright (C) Dennis M. Akos
# Written by Darius Plausinaitis and Dennis M. Akos
# Based on code by DMAkos Oct-1999
#--------------------------------------------------------------------------
#This program is free software; you can redistribute it and/or
#modify it under the terms of the GNU General Public License
#as published by the Free Software Foundation; either version 2
#of the License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
#USA.
#--------------------------------------------------------------------------

import numpy as np
import matplotlib.pyplot as plt
import Acquisition

import GoldCode
from GPSData import IQData

#np.set_printoptions(threshold=np.inf)

def main():
    # Import data. Will read many ms at once, then process the blocks as needed.
    # Need these to pass to importFile module
    fs = 4.092*10**6 # Sampling Frequency [Hz]
    numberOfMilliseconds = 450
    sampleLength = numberOfMilliseconds*10**(-3)
    bytesToSkip = 0

    data = IQData()
    # Uncomment one of these lines to choose between Launch12 or gps-sdr-sim data

    # /home/evan/Capstone/gps/resources/JGPS@-32.041913222
    #data.importFile('resources/JGPS@04.559925043', fs, sampleLength, bytesToSkip)
    #data.importFile('resources/JGPS@-32.041913222', fs, sampleLength, bytesToSkip)
    #data.importFile('resources/test4092kHz.max', fs, sampleLength, bytesToSkip)
    data.importFile('resources/Single4092KHz5s.max', fs, sampleLength, bytesToSkip)
    
    acqresult = Acquisition.SatStats()
    acqresult.CodePhaseSamples = int((1023.0 - 630.251585)*4 + 1)
    acqresult.FineFrequencyEstimate = -3340
    acqresult.Sat = 1


    channel1 = Channel(data, acqresult)
    channel1.Track()
    channel1._writeBits()



class Channel:
    def __init__(self, datain, acqData, chartoutput = True):
        #Acquisition inputs
       
        self.data = datain
        self.codePhase = acqData.CodePhaseSamples
        self.acquiredCarrFreq = acqData.FineFrequencyEstimate
        self.PRN = acqData.Sat # Value will be non-zero if Acquisition was successful for this channel


        self.progress = True #Output progress
        self.status = False # True if tracking was successful, False otherwise.

        #Tracking Parameters (these should be moved to a .json,.xml,or .conf soon)
        self.msToProcess = 430 # How many ms blocks to process per channel
        self.earlyLateSpacing = 0.5 # How many chips to offset for E & L codes.
        self.codeLoopNoiseBandwidth = 2 # [Hz]
        self.codeZeta = 0.7
        self.codeLoopGain = 1.
        self.carrLoopNoiseBandwidth = 25 # 25 [Hz]
        self.carrZeta = 0.7
        self.carrLoopGain = 0.25 # 0.25
        self.codeFreqBasis = 1.023*10**6 # L1 C/A Code frequency
        self.samplingFreq = 4.092*10**6 # Sampling frequency of ADC
        self.codeLength = 1023
        self.SamplesPerChip = int(self.samplingFreq/self.codeFreqBasis)

        self.PDIcode = .001
        self.PDIcarr = .001


        #Tracking Result/Logging Parameters
        self.outputChart = chartoutput
        if chartoutput:
            #Preallocate space if charts are requested
            self.absoluteSample = np.zeros((self.msToProcess)) # Sample that C/A code 1st starts.
            self.codeFreq = np.zeros((self.msToProcess)) # C/A code frequency.
            self.carrFreq = np.zeros((self.msToProcess)) # Frequency of tracked carrier.
            self.I_P  = np.zeros((self.msToProcess)) # Correlator outputs (resulting sum).
            self.I_E  = np.zeros((self.msToProcess)) # Correlator outputs (resulting sum).
            self.I_L  = np.zeros((self.msToProcess)) # Correlator outputs (resulting sum).
            self.Q_P  = np.zeros((self.msToProcess)) # Correlator outputs (resulting sum).
            self.Q_E  = np.zeros((self.msToProcess)) # Correlator outputs (resulting sum).
            self.Q_L  = np.zeros((self.msToProcess)) # Correlator outputs (resulting sum).
            self.dllDiscr = np.zeros((self.msToProcess)) # Code-Loop discriminator
            self.dllDiscrFilt = np.zeros((self.msToProcess)) # Code-Loop discriminator filter
            self.pllDiscr = np.zeros((self.msToProcess)) # Carrier-Loop discriminator
            self.pllDiscrFilt = np.zeros((self.msToProcess)) # Carrier-Loop discriminator filter
    
    def Track(self):

        # Calculate filter coefficient values for code loop
        tau1code, tau2code = self._calcLoopCoef(self.codeLoopNoiseBandwidth, self.codeZeta, self.codeLoopGain)

        # Calculate filter coefficient values for carrier loop
        tau1carr, tau2carr = self._calcLoopCoef(self.carrLoopNoiseBandwidth, self.carrZeta, self.carrLoopGain)

        # Process each channel (Will impliment loop in future. For now only processing one channel)
        # Process channel if PRN is non-zero (Acquisition successful)
        if self.PRN:
            # Create instance of TrackingResults to store results into
         

            CACode = GoldCode.getTrackingCode(self.PRN)

            # Perform additional initializations:
            codeFreq = self.codeFreqBasis

            # Residual code phase (Chips)
            remCodePhase = 0.0

            # define residual carrier phase
            remCarrPhase  = 0.0

            # code tracking loop parameters
            oldCodeNco   = 0.0
            oldCodeError = 0.0

            # carrier/Costas loop parameters
            oldCarrNco   = 0.0
            oldCarrError = 0.0

            dataPosition = 0
            blksize = 0

            carrFreq = self.acquiredCarrFreq

            # Process the requested number of code periods (num of ms to process)
            for loopCount in range(0, self.msToProcess):
                if self.progress:
                    print("------- %2.1f perecnt complete --------"%((loopCount/self.msToProcess)*100), end = '\r')
                # Read current block of data
                # Find the size of a "block" or code period in whole samples

                # Update the phasestep based on code freq (variable) and
                # sampling frequency (fixed)
                codePhaseStep = np.real(codeFreq / self.samplingFreq)

                #print("Old blksize: %d"%blksize)
                blksize = int(np.ceil((self.codeLength-remCodePhase) / codePhaseStep))
                #print("New blksize: %d"%blksize)
                #print("Old remCodePhase: %f" %remCodePhase)

                # Read in the appropriate number of samples to process this
                # iteration
                rawSignal = self.data.IData[self.codePhase + dataPosition: self.codePhase + dataPosition + blksize]
                dataPosition = dataPosition + blksize


                # Generate Early CA Code.
                tStart = remCodePhase - self.earlyLateSpacing
                tStep = codePhaseStep
                tEnd = ((blksize-1)*codePhaseStep+remCodePhase) + codePhaseStep - self.earlyLateSpacing
                tcode = np.linspace(tStart,tEnd,blksize,endpoint=False)
                tcode2 = (np.ceil(tcode)).astype(int)
                earlyCode = CACode[tcode2]


                # Generate Late CA Code.
                tStart = remCodePhase + self.earlyLateSpacing
                tStep = codePhaseStep
                tEnd = ((blksize-1)*codePhaseStep+remCodePhase) + codePhaseStep + self.earlyLateSpacing
                tcode = np.linspace(tStart,tEnd,blksize,endpoint=False)
                tcode2 = (np.ceil(tcode)).astype(int)
                lateCode = CACode[tcode2]


                # Generate Prompt CA Code.
                tStart = remCodePhase
                tStep = codePhaseStep
                tEnd = ((blksize-1)*codePhaseStep+remCodePhase) + codePhaseStep
                tcode = np.linspace(tStart,tEnd,blksize,endpoint=False)
                tcode2 = (np.ceil(tcode)).astype(int)
                promptCode = CACode[tcode2]


                # Figure out remaining code phase (uses tcode from Prompt CA Code generation):
                remCodePhase = (tcode[blksize-1]) - 1023.00
                if abs(remCodePhase) > codePhaseStep:
                    remCodePhase = sign(remCodePhase)*codePhaseStep
                else:
                    remCodePhase = 0
                #print("remCodePhase: %f" %remCodePhase)
                # The line above is not working properly. I believe the tcode array is
                # not correct, but will need to do some debugging, therefore the
                # remCodePhase is set to zero below, for the time being.
                #remCodePhase = 0
                #print("New remCodePhase: %12.8f" %remCodePhase)

                # Generate the carrier frequency to mix the signal to baseband
                #time    = np.linspace(0, blksize/self.samplingFreq, blksize+1, endpoint=True)
                time = np.array(range(0,blksize+1))/self.samplingFreq

                #print("Length of time array for cos and sin: %d" %len(time))
                # Get the argument to sin/cos functions
                trigarg = ((carrFreq * 2.0 * np.pi) * time) + remCarrPhase
                remCarrPhase = trigarg[blksize] % (2 * np.pi)

                # Finally compute the signal to mix the collected data to baseband
                carrCos = np.cos(trigarg[0:blksize])
                carrSin = np.sin(trigarg[0:blksize])

                # First mix to baseband
                qBasebandSignal = carrCos * rawSignal
                iBasebandSignal = carrSin * rawSignal

                # Now get early, late, and prompt values for each
                I_E = np.sum(earlyCode  * iBasebandSignal)
                Q_E = np.sum(earlyCode  * qBasebandSignal)
                I_P = np.sum(promptCode * iBasebandSignal)
                Q_P = np.sum(promptCode * qBasebandSignal)
                I_L = np.sum(lateCode   * iBasebandSignal)
                Q_L = np.sum(lateCode   * qBasebandSignal)

                # Find PLL error and update carrier NCO
                # Implement carrier loop discriminator (phase detector)
                carrError = np.arctan(Q_P / I_P) / (2.0 * np.pi)

                # Implement carrier loop filter and generate NCO command
                carrNco = oldCarrNco + (tau2carr/tau1carr) * (carrError - oldCarrError) + carrError * (self.PDIcarr/tau1carr)
                oldCarrNco   = carrNco
                oldCarrError = carrError

                # Modify carrier freq based on NCO command
                carrFreq = self.acquiredCarrFreq + carrNco

                # Find DLL error and update code NCO -------------------------------------
                codeError = (np.sqrt(I_E * I_E + Q_E * Q_E) - np.sqrt(I_L * I_L + Q_L * Q_L)) /\
                            (np.sqrt(I_E * I_E + Q_E * Q_E) + np.sqrt(I_L * I_L + Q_L * Q_L))

                # Implement code loop filter and generate NCO command
                codeNco = oldCodeNco + (tau2code/tau1code) * (codeError - oldCodeError) + codeError * (self.PDIcode/tau1code)
                oldCodeNco   = codeNco
                oldCodeError = codeError

                # Modify code freq based on NCO command
                codeFreq = self.codeFreqBasis - codeNco

                if self.outputChart:
                    self.pllDiscr[loopCount] = carrError
                    self.carrFreq[(loopCount)] = carrFreq # Return real value only?

                    self.codeFreq[(loopCount)] = codeFreq
                    self.I_E[loopCount] = I_E
                    self.I_P[loopCount] = I_P
                    self.I_L[loopCount] = I_L
                    self.Q_E[loopCount] = Q_E
                    self.Q_P[loopCount] = Q_P
                    self.Q_L[loopCount] = Q_L

            if self.outputChart:
                self._plotOutputs()
                

    def _plotOutputs(self):
        plt.plot(self.carrFreq)
        plt.ylabel("PLL Frequency (Hz)")
        plt.xlabel("t (ms)")
        plt.title("Carrier frequency of NCO")
        plt.show()
        
        plt.subplot(2,1,1)
        plt.plot(self.I_E**2,label="I_E")
        plt.plot(self.I_P**2,label="I_P")
        plt.plot(self.I_L**2,label="I_L")
        plt.title("DLL Inphase")
        plt.legend()
        
        plt.subplot(2,1,2)
        plt.plot(self.Q_E**2,label="Q_E")
        plt.plot(self.Q_P**2,label="Q_P")
        plt.plot(self.Q_L**2,label="Q_L")
        plt.title("DLL Quadrature")
        plt.xlabel("t (ms)")
        plt.show()

        SatelliteData = self.I_P
        for ind,IP in enumerate(SatelliteData):
            if IP > 0.1:
                SatelliteData[ind] = 1
            elif IP < 0.1:
                SatelliteData[ind] = 0

        plt.plot(SatelliteData)
        plt.ylim([-.5,1.5])
        plt.title("50bps Navigation Data (from I_P)")
        plt.show()

        #plt.plot(self.pllDiscr)
        #plt.show()

    def _calcLoopCoef(self, LoopNoiseBandwidth, Zeta, LoopGain):
        '''
        Calculates the loop coefficients tau1 and tau2
        '''
        # Solve for the natural frequency
        Wn = LoopNoiseBandwidth*8*Zeta / (4*Zeta**2 + 1)

        # Solve for tau1 and tau2
        tau1 = LoopGain / (Wn * Wn);
        tau2 = (2.0 * Zeta) / Wn;

        return (tau1, tau2)

    def _writeBits(self, dr = '.', name = 'default'):
        '''
        Writes out the navigation data bits to a file
        '''

        if name == 'default':
            name = 'SV%s.bin'%self.PRN
        
        # First find a bit transition to be the starting index of the integration
        start = np.sign(self.I_P[0])

        startInd = 0
        for samp in self.I_P:
            if (np.sign(samp) != start ):
                break
            else:
                startInd += 1
            
        #Start integrating bits in groups of 20ms
        ptr = 0
        bits = np.zeros(len(self.I_P)/20)

        for ind in range(startInd, len(self.I_P), 20):
            m = np.mean(self.I_P[ind:ind+20])
            
            if np.sign(m) == 1:
                bits[ptr] = 1
            elif np.sign(m) == -1:
                bits[ptr] = 0
            else:
                pass
                #raise BitsError(ind)
            ptr += 1
        
        #Write out the ms offset, followed by bitstream
        with open( '%s/%s'%(dr, name),'w') as f:
            f.write("%1d"%startInd) 
            f.writelines(["%3d" % item  for item in bits])
            print()
            print("File written to: %s"%f.name)
                

        

class BitsError(Exception):
    def __init__(self, index):
        self.message = "Integration Error at index %d"%index

        print(self.message)



if __name__ == "__main__":
    main()