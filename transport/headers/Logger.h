/*
 * Logger.h
 *
 *  Created on: Mar 5, 2011
 *      Author: erickson
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <pantheios/pantheios.hpp>      /* The root header for Panthieos when using the C++-API. */
#include <pantheios/inserters.hpp>      /* Includes all headers for inserters, including integer, real, character */
#include <pantheios/frontends/stock.h>  /* Declares the process identity symbol PANTHEIOS_FE_PROCESS_IDENTITY */
#include <pantheios/backends/bec.file.h>      //print to file
#include <pantheios/backends/bec.fprintf.h>		//print to console

#define PantheiosString(x) PANTHEIOS_LITERAL_STRING(x)

using namespace pantheios;

#endif /* LOGGER_H_ */
