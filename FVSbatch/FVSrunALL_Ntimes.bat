@echo OFF


SET /A n = 10
REM user must set n = how many times each keyfile must be run


 

REM CHANGE NOTHING BELOW THIS LINE
FOR %%G IN (*.key) DO (
	FOR /L %%F IN (1, 1, %n%) DO (
		FVSne --keywordfile=%%G
	)
)
