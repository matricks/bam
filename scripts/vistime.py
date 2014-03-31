#!/usr/bin/env python

# Gant-chart style build time visualization tool for bam event logs. Usefull to find things preventing
# good parallelization. Not very done, not very nice.
#
# build:
# bam --debug-eventlog evlog.txt
# Run: 
# vistime.py evlog.txt
#
# Written by Markus Alind (markus.alind at gmail.com)


import sys
import os
import random
import copy
import math

import Tkinter
import tkMessageBox


def TimeToStr( val ):
	if val >= 1.0:
		return "% 3.2fs " % val
	elif val >= 0.001:
		return "% 3.2fms" % ( val * 1000 )
	else:
		return "% 3.2fus" % ( val * 1000**2 )


class Job:
	def __init__( self, name, start, thread ):
		self.name = name
		self.start = start
		self.end = None
		self.thread = thread		
	def SetEnd( self, end ) :
		self.end = end
	def GetRunTime( self ):
		if self.end :
			return self.end - self.start

class JobThread:
	def __init__ ( self ):
		self.cur = None
		self.all = []

class Jobs:
	def __init__ ( self ):
		self.map = {}
		self.allJobs = []
		self.threads = {}
		
	def ParseLine( self, line ):
		line = line.strip().split( None , 3 )
		thread = int( line[ 0 ] )
		time = float( line[ 1 ] )
		action = line[ 2 ]
		name = line[ 3 ]
		if name == "build:":
			return
		if not self.threads.has_key( thread ):
			self.threads[ thread ] = JobThread()
		th = self.threads[ thread ]
		if ( action == "begin" ):			
			assert ( not th.cur )
			th.cur = Job( name, time, thread )
			th.all.append( th.cur )
			self.allJobs.append( th.cur )
		elif ( action == "end" ) :
			assert( th.cur )
			th.cur.SetEnd( time )
			th.cur = None
		else:
			assert( False )
		
	def Parse( self, lines ):
		for line in lines:
			if line.strip():
				self.ParseLine( line )
	def GetThreads( self ):
		res = self.threads.keys()
		res.sort()
		return res


class TkGui:
	def __init__( self, jobs = None ):
		
		self.axisGraphics = []
		self.CreateWindow( ( 800, 800 ) )
		self.canvasSize = ( 800, 800 )
				
		self.CalcSizes( 32 )
		
		self.InitCanvas( jobs )		
	
	def WindowLoop( self ):
		self.win.mainloop()
	
	def CalcSizes( self, xScale ):
		#xSize = 2000
	
		self.yRectSize = 20
		self.ySpacing = self.yRectSize * 1.25		
		self.xScale = xScale
		self.xMargin = 30
		self.xStart = self.xMargin
		self.yMargin = self.ySpacing * 2		
		self.timeOffset = 0
		self.yStart = self.yMargin

		
		#rects = []
		#minTime = 0
		#maxTime = 0
		
		#~ for t in jobs.GetThreads():
			#~ th = jobs.threads[ t ]
			#~ minTime = min( minTime, th.all[ 0 ].start )
			#~ maxTime = max( maxTime, th.all[ -1 ].end )
		
		#~ numThreads = len( jobs.threads )
		#~ ySize = ( numThreads ) * ySpacing + yMargin*2
		#~ yStart = yMargin
		
		#~ totTime = maxTime - minTime
		#~ xScale = ( xSize - xMargin*2 ) / totTime
		#~ xStart = xMargin

	
	def CreateWindow( self, canvasSize ):
		self.win = Tkinter.Tk( )
		
		self.statusbar = Tkinter.Label( self.win, text="", bd=1, relief=Tkinter.SUNKEN, anchor=Tkinter.W )
		self.statusbar.pack( side=Tkinter.BOTTOM, fill=Tkinter.X )
		
		self.scrollbarx = Tkinter.Scrollbar( self.win, orient=Tkinter.HORIZONTAL, width=24 )
		self.scrollbarx.grid(column=0, row=1, sticky=(Tkinter.W,Tkinter.E))
		self.scrollbarx.pack( side = Tkinter.BOTTOM, fill=Tkinter.X )
		
		self.canvas = Tkinter.Canvas( self.win, bg="lightgray",
			width = canvasSize[ 0 ], height = canvasSize[ 1 ], 
			scrollregion=( 0, 0, canvasSize[ 0 ], canvasSize[ 1 ] ),
			xscrollcommand=self.scrollbarx.set 
			)
		self.canvas.grid( column=0, row=0, sticky=( Tkinter.N, Tkinter.W, Tkinter.E, Tkinter.S ) )
		
		
		self.scrollbarx.config( command = self.canvas.xview )
		
		#self.canvas.config(scrollregion=canvas.bbox(Tkinter.ALL))
		
		self.canvas.pack( side = Tkinter.TOP, fill=Tkinter.X )
		
		self.canvas.tag_bind( "jobrect", "<Enter>", self.Ev_RectEnter )	
		self.canvas.tag_bind( "jobrect", "<Leave>", self.Ev_RectLeave )
		self.canvas.bind( '<Button-5>', lambda e: self.Ev_Zoom( e, -1 ) )
		self.canvas.bind( '<Button-4>', lambda e: self.Ev_Zoom( e, 1 ) )
		self.canvas.bind( '<Button-3>', self.Ev_MButtonRightDown )
		self.canvas.bind( '<B3-Motion>', self.Ev_MButtonRightMove )
		self.win.bind( '<MouseWheel>', self.Ev_ZoomWheel )
		

	def UpdateCanvasSize( self, newSize ):
		maxX = newSize[ 0 ] + self.xMargin
		maxY = newSize[ 1 ] + self.yMargin
		if not maxX == self.canvasSize[ 0 ] or not maxY == self.canvasSize[ 1 ]:
			self.canvasSize = ( maxX, maxY )
			self.canvas.config( scrollregion=( 0, 0, maxX, maxY ), )
			#self.canvas.config( width=maxX, height=maxY, scrollregion=( 0, 0, maxX, maxY ), )
	
	def UpdateAxis( self ):
		# we just wipe everything
		for obj in self.axisGraphics:
			self.canvas.delete( obj )
		self.axisGraphics = []
		tbY = self.yMargin - self.ySpacing
		tTickY = tbY - 2
		tMajorY = 8
		tMinorY = 5
		tMinorPerMajor = 4
		tMajorXTarget = 100
		
		tbStartX = self.xMargin
		tbStopX = self.canvasSize[ 0 ] - self.xMargin
		
		baseLine = self.canvas.create_line( tbStartX, tbY, tbStopX, tbY )
		self.axisGraphics.append( baseLine )
		
		#print ( "self.timeMax,", self.timeMax, "tbStopX - tbStartX,", tbStopX - tbStartX )
		secPerMajor = self.timeMax / ( ( tbStopX - tbStartX ) / tMajorXTarget )
		#print ( "secPerMajor", secPerMajor )
		
		
		#if secPerMajor < 1 :
		#	secPerMajor = 1
		if secPerMajor < 30 :
			if self.timeMax / secPerMajor > 1000 :
				secPerMajor = self.timeMax / 1000
			tenPot = math.floor( math.log10( secPerMajor ) )
			
			
			norm = secPerMajor / ( 10**tenPot )
			#print norm
			for i in [ 5, 2, 1 ]:
				if norm > i :
					norm = i
					break
			secPerMajor = norm * ( 10**tenPot )
		elif secPerMajor < 60 :
			secPerMajor = 30
		else:
			minPerMajor = secPerMajor / 60
			tenPot = math.floor( math.log10( minPerMajor ) )
			norm = minPerMajor / ( 10**tenPot )
			for i in [ 5, 2, 1 ]:
				if norm > i :
					norm = i
					break
			minPerMajor = norm * ( 10**tenPot )
			secPerMajor = minPerMajor * 60
		
		
		#print ( "secPerMajor", secPerMajor )
		
		
		numMajor = int( math.floor( self.timeMax / secPerMajor ) )
		#print ( "numMajor", numMajor )
		for m in range( numMajor + 1 ):
			majorPos = tbStartX + m * secPerMajor * self.xScale
			self.axisGraphics.append( self.canvas.create_line( majorPos, tTickY, majorPos, tTickY + tMajorY ) )
			time = m*secPerMajor
			self.axisGraphics.append( self.canvas.create_text( majorPos, tTickY, anchor=Tkinter.S, text=TimeToStr(time) ) )
			for minor in range(1, tMinorPerMajor ) :
				minorPos = majorPos + minor * ( secPerMajor / tMinorPerMajor ) * self.xScale
				if ( minorPos <= tbStopX ):
					self.axisGraphics.append( self.canvas.create_line( minorPos, tTickY, minorPos, tTickY + tMinorY ) )
		#print len( self.axisGraphics )
			
	
		
		
		

	class JobRect:
		def __init__( self, job ):
			self.job = job
			self.rect = None
	
	def CalcRect(self, job ):
		x0 = ( job.start - self.timeOffset ) * self.xScale +self.xStart
		x1 = ( job.end - self.timeOffset ) * self.xScale + self.xStart
		y0 = ( job.thread*self.ySpacing - self.yRectSize*0.5) + self.yStart
		y1 = ( job.thread*self.ySpacing + self.yRectSize*0.5) + self.yStart
		return ( x0, y0, x1, y1 )
	
	def InitCanvas( self, jobs ):
		self.guiIdToJob = {}
		self.jobToGuiId = {}
		self.jobRects = []
		
		maxX = 0
		maxY = 0
		timeMax = 0
		for t in jobs.GetThreads():
			th = jobs.threads[ t ]
			for j in th.all:
				
				jr = self.JobRect( j )
				( x0, y0, x1, y1 ) = self.CalcRect( jr.job )
								
				maxX = max( maxX, x1 )
				maxY = max( maxY, y1 )
				
				timeMax = max( timeMax, jr.job.end )
				
				#~ jr.rect = self.canvas.create_polygon(  
					#~ x0, y0,
					#~ x0, y1,
					#~ x1, y1,
					#~ x1, y0,
					#~ fill="lightgreen", outline="black", activefill="green", activeoutline="black", tag="jobrect" )
				col = ( "lightgreen", "green" )
				if not "job:" in j.name and ( "cache load"  in j.name or "script parse" in j.name or "prepare" in j.name ):
					col = ( "lightcyan", "cyan" )
				elif not ("c " in j.name and "c++" in j.name ) and "link" in j.name:
					col = ( "lightblue", "blue" )
				elif "job:" in j.name and "precomp" in j.name :
					col = ( "lightyellow", "yellow" )
				elif "job:" in j.name and ".dll" in j.name :
					col = ( "lightblue", "blue" )
				
				jr.rect = self.canvas.create_rectangle(  
					( x0, y0, x1, y1 ),
					fill=col[0], outline="black", activefill=col[1], activeoutline="black", tag="jobrect" )
				self.guiIdToJob[ jr.rect ] = jr
				self.jobToGuiId[ j.name ] = jr
				self.jobRects.append( jr )
				#canvas.tag_bind( rect, "<Enter>", test )
			self.timeMax = timeMax
		self.UpdateCanvasSize( ( maxX, maxY ) )
		self.UpdateAxis()
	
	def UpdateCanvas( self ):
		maxX = 0
		maxY = 0
		for jr in self.jobRects:
			( x0, y0, x1, y1 ) = self.CalcRect( jr.job )
			maxX = max( maxX, x1 )
			maxY = max( maxY, y1 )
			
			self.canvas.coords( jr.rect, ( x0, y0, x1, y1 ) )
				
		self.UpdateCanvasSize( ( maxX, maxY ) )
		self.UpdateAxis()
		
	
	def SetScale( self, xScale ):
		self.CalcSizes( xScale )
		self.UpdateCanvas()
		
	def CenterAtTime( self, time ):
		winWidth = self.canvas.winfo_width()
		xNewCenter = ( time*self.xScale ) / ( self.canvasSize[ 0 ] - self.xMargin*2 )  - ( winWidth / self.canvasSize[ 0 ] ) / 2		
		self.canvas.xview_moveto( xNewCenter )
		
		
	def Ev_RectEnter( self, event ):
		curId = self.canvas.find_withtag( Tkinter.CURRENT )
		jr = self.guiIdToJob[ curId[ 0 ] ]
		text = "%s %s" % ( TimeToStr( jr.job.GetRunTime() ), jr.job.name )
		self.statusbar.config( text=text )
		
		#print guiIdToJob[ Tkinter.CURRENT ]
		#print event
		#print dir( event )
		#print event.num
		#canvas.itemconfigure( Tkinter.CURRENT, fill="red" )
		
	def Ev_RectLeave( self, event ):
		self.statusbar.config( text="" )
		#print event
		#print dir( event )
		#print event.num
		#canvas.itemconfigure( Tkinter.CURRENT, fill="lightgreen" )
		
	def Ev_Zoom( self, event, val ):
		# zoom but keep the spot under the cursor under the cursor 
		xCurWindow = event.x +0.5
		xCurCanvas = self.canvas.canvasx( xCurWindow )
		xCenterTime = ( xCurCanvas - self.xStart ) / self.xScale
		#print "time:", xCenterTime
		scale = self.xScale*0.75**(-val)
		self.SetScale( scale )
		
		#print self.canvasSize
		#print "width", self.canvas.winfo_width()
		winWidth = self.canvas.winfo_width()
		xCenterPixel =  xCenterTime*self.xScale + self.xStart - (xCurWindow - winWidth/2 ) 
		xCenterFrac = xCenterPixel / ( self.canvasSize[ 0 ] )  - ( winWidth / self.canvasSize[ 0 ] ) / 2
		#print xCenterPixel
		#print "res:", xCenterFrac
		self.canvas.xview_moveto( xCenterFrac )
		
	def	Ev_ZoomWheel( self, event ) :
		self.Ev_Zoom( event, event.delta/120 )
		
	def Ev_MButtonRightDown( self, event ):
		self.canvas.scan_mark( event.x, 0 )
		
	def Ev_MButtonRightMove( self, event ):
		self.canvas.scan_dragto( event.x, 0, 1 )


	
def callback(event):
	canvas = event.widget
	x = canvas.canvasx(event.x)
	y = canvas.canvasy(event.y)	
	print canvas.find_closest(x, y)



def main( argv ):
	#bam --debug-eventlog log.txt
	#com = argv[ 1 ]
	logPath = argv[ 1 ]
	jobs = Jobs( )
	jobs.Parse( open( logPath ).readlines() )
	
#	allSorted = sorted( jobs.allJobs, key = lambda j : j.GetRunTime(), reverse = True )
	
#	for j in allSorted:		
#		print( "%s, % 2d, %s" % ( TimeToStr( j.GetRunTime() ), j.thread, j.name ) )
		
	
	w = TkGui( jobs )
	w.WindowLoop()	
	


if __name__ == "__main__":
	main( sys.argv )