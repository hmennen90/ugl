/*
   Product name and file names of the UGL build.

   The PHP License 3.01 and the Zend License 2.00 do not allow derived products to carry
   "PHP" or "Zend" in their name. Everything a user sees as the name of this product comes
   from here. Copyright notices and the acknowledgment
   "This product includes PHP software, freely available from https://www.php.net/software/"
   stay as they are (see README.UGL.md).
*/

#ifndef ZEND_UGL_BRANDING_H
#define ZEND_UGL_BRANDING_H

#ifdef ZEND_UGL
# define UGL_PRODUCT_NAME     "UGL"
# define UGL_PRODUCT_VERSION  "0.1.0-dev"
# define UGL_INI_FILENAME     "ugl.ini"
# define UGL_INI_SAPI_FORMAT  "ugl-%s.ini"
#endif

#endif /* ZEND_UGL_BRANDING_H */
