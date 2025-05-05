const jwt = require('jsonwebtoken');

const JWT_SECRET = '1234567890';

const verifyToken = (req, res, next) => {
    const authHeader = req.headers['authorization'];
    const token = authHeader && authHeader.startsWith('Bearer ') 
        ? authHeader.split(' ')[1] 
        : (req.cookies ? req.cookies.token : null);

    if (!token) {
        return res.status(401).json({ error: 'Token puuttuu, autentikointi vaaditaan' });
    }

    try {
        const decoded = jwt.verify(token, JWT_SECRET);
        req.user = decoded;
        next();
    } catch (error) {
        console.error('Error verifying token:', error.message);
        return res.status(401).json({ error: 'Virheellinen tai vanhentunut token' });
    }
};

module.exports = { verifyToken };