#pragma once

class CElevate  
{
public:
	CElevate();
	virtual ~CElevate();

public:
	static bool AutoElevate(void);

public:
	static bool IsWindowsVistaLater();
	static bool IsElevated( bool * pbElevated );
};
